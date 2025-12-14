package com.arduos;

import com.fazecast.jSerialComm.SerialPort;
import streamprotocol.ParsedPacket;
import streamprotocol.StreamProtocol;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Scanner;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class Main {
    private static SerialPort serialPort;
    private static final StreamProtocol protocol = new StreamProtocol();
    
    // Command IDs (Must match Arduino Protocol.h)
    public static final int SYS_LS      = 1;
    public static final int SYS_EXEC    = 2;
    public static final int SYS_CHDIR   = 3;
    public static final int SYS_GETCWD  = 4;

    public static final int CMD_STDIN   = 100;
    public static final int CMD_STDOUT  = 101;
    public static final int CMD_STDERR  = 102;
    public static final int CMD_PING    = 200;

    // Payload Types
    public static final int PT_NONE     = 0;
    public static final int PT_STRING   = 1;

    public static void main(String[] args) {
        System.out.println("=== ArduOS Client v2.0 ===");
        
        // 1. 시리얼 포트 선택
        SerialPort[] ports = SerialPort.getCommPorts();
        if (ports.length == 0) {
            System.out.println("No serial ports found!");
            return;
        }

        System.out.println("Available Ports:");
        for (int i = 0; i < ports.length; i++) {
            System.out.println((i + 1) + ". " + ports[i].getSystemPortName() + " (" + ports[i].getDescriptivePortName() + ")");
        }

        Scanner scanner = new Scanner(System.in);
        System.out.print("Select port number: ");
        int choice = scanner.nextInt();
        scanner.nextLine(); // consume newline

        if (choice < 1 || choice > ports.length) {
            System.out.println("Invalid choice.");
            return;
        }

        serialPort = ports[choice - 1];
        serialPort.setBaudRate(9600); // 아두이노와 동일하게 맞춤
        serialPort.setComPortTimeouts(SerialPort.TIMEOUT_READ_BLOCKING, 100, 0);

        if (serialPort.openPort()) {
            System.out.println("Connected to " + serialPort.getSystemPortName());
        } else {
            System.out.println("Failed to open port.");
            return;
        }

        // 2. 수신 스레드 시작
        Thread receiverThread = new Thread(Main::receiveLoop);
        receiverThread.setDaemon(true);
        receiverThread.start();

        // 3. 메인 루프 (명령어 입력)
        printHelp();
        while (true) {
            // 프롬프트 출력 (비동기 출력이 섞일 수 있음)
            try { Thread.sleep(100); } catch (InterruptedException e) {} 
            System.out.print("> ");
            
            String input = scanner.nextLine().trim();

            if (input.equalsIgnoreCase("exit")) break;
            if (input.isEmpty()) continue;

            processInput(input);
        }

        serialPort.closePort();
        System.out.println("Disconnected.");
    }

    private static void receiveLoop() {
        List<Byte> tempBuffer = new ArrayList<>();
        byte[] readBuf = new byte[1]; 
        
        int state = 0;
        long expectedLen = 0;
        
        while (serialPort.isOpen()) {
            if (serialPort.bytesAvailable() > 0) {
                int n = serialPort.readBytes(readBuf, 1);
                if (n > 0) tempBuffer.add(readBuf[0]);
                
                if (state == 0) {
                    if (tempBuffer.size() >= 8) {
                        long headerLow = (tempBuffer.get(0) & 0xFFL) | ((long)(tempBuffer.get(1) & 0xFFL) << 8) | ((long)(tempBuffer.get(2) & 0xFFL) << 16) | ((long)(tempBuffer.get(3) & 0xFFL) << 24);
                        long headerHigh = (tempBuffer.get(4) & 0xFFL) | ((long)(tempBuffer.get(5) & 0xFFL) << 8) | ((long)(tempBuffer.get(6) & 0xFFL) << 16) | ((long)(tempBuffer.get(7) & 0xFFL) << 24);
                        long headerValue = headerLow | (headerHigh << 32);
                        
                        expectedLen = (headerValue >> 4) & 0x1FFFFFFFFFFFL;
                        
                        if (expectedLen > 1024 || expectedLen < 8) {
                            tempBuffer.remove(0); // 유효하지 않으면 1바이트 버림
                        } else {
                            state = 1; // 바디 읽기
                        }
                    }
                } else if (state == 1) {
                    if (tempBuffer.size() >= expectedLen) {
                        byte[] packetData = new byte[(int)expectedLen];
                        for(int i=0; i<(int)expectedLen; i++) packetData[i] = tempBuffer.get(i);
                        
                        try {
                            ParsedPacket parsed = protocol.parsePacket(packetData);
                            handlePacket(parsed);
                        } catch (Exception e) {
                            // 파싱 에러 무시
                        }
                        
                        tempBuffer.subList(0, (int)expectedLen).clear();
                        state = 0;
                        expectedLen = 0;
                    }
                }
            } else {
                try { Thread.sleep(5); } catch (InterruptedException e) {}
            }
        }
    }

    private static void processInput(String input) {
        String[] parts = input.split("\\s+", 2);
        String cmd = parts[0].toLowerCase();
        String arg = (parts.length > 1) ? parts[1] : "";

        try {
            byte[] packet = null;

            switch (cmd) {
                case "ping":
                    // Removed
                    break;
                case "ls":
                    if (!arg.isEmpty()) {
                        packet = protocol.toBytes(arg.getBytes(StandardCharsets.UTF_8), StreamProtocol.UNFRAGED, (byte)PT_STRING, SYS_LS);
                    } else {
                        packet = protocol.toBytes(new byte[0], StreamProtocol.UNFRAGED, (byte)PT_NONE, SYS_LS);
                    }
                    break;
                case "exec":
                case "run":
                    if (arg.isEmpty()) {
                        System.out.println("Usage: exec <filename>");
                        return;
                    }
                    packet = protocol.toBytes(arg.getBytes(StandardCharsets.UTF_8), StreamProtocol.UNFRAGED, (byte)PT_STRING, SYS_EXEC);
                    break;
                case "cd":
                    if (arg.isEmpty()) {
                        System.out.println("Usage: cd <path>");
                        return;
                    }
                    packet = protocol.toBytes(arg.getBytes(StandardCharsets.UTF_8), StreamProtocol.UNFRAGED, (byte)PT_STRING, SYS_CHDIR);
                    break;
                case "pwd":
                    packet = protocol.toBytes(new byte[0], StreamProtocol.UNFRAGED, (byte)PT_NONE, SYS_GETCWD);
                    break;
                default:
                    System.out.println("Unknown command. (Try: ls, exec, cd, pwd, exit)");
                    return;
            }

            if (packet != null) {
                serialPort.writeBytes(packet, packet.length);
                // [수정] 강제 지연 제거 (비동기 처리)
                // 대신 프롬프트 출력을 위해 잠깐만 대기 (옵션 - 200ms)
                try { Thread.sleep(200); } catch (InterruptedException e) {}
            }
        } catch (Exception e) {
            System.err.println("Error encoding packet: " + e.getMessage());
        }
    }

    private static void handlePacket(ParsedPacket p) {
        int cmd = p.getUserField();
        String payloadStr = new String(p.getPayload(), StandardCharsets.UTF_8);

        switch (cmd) {
            case CMD_STDOUT:
                System.out.print(payloadStr); // 아두이노 출력
                break;
            case CMD_STDERR:
                System.err.print(payloadStr); // 아두이노 에러
                break;
            default:
                // System.out.println("[Rx] Cmd: " + cmd + ", Data: " + payloadStr);
                break;
        }
    }

    private static void printHelp() {
        System.out.println("Commands: exec <file>, ls, cd <path>, pwd, ping, exit");
    }
}
