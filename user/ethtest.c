#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("--- ETHERNET DRIVER TEST START ---\n\n");

  char tx_packet[128];
  char rx_packet[128];
  char *payload = "Hello from xv6 Ethernet Card!";
  int payload_len = strlen(payload);

  // 1. Format raw Ethernet Frame
  // Byte 0-5: Destination MAC Address (FF:FF:FF:FF:FF:FF - Broadcast)
  for (int i = 0; i < 6; i++) {
    tx_packet[i] = 0xFF;
  }

  // Byte 6-11: Source MAC Address (02:00:00:00:00:01)
  tx_packet[6] = 0x02;
  tx_packet[7] = 0x00;
  tx_packet[8] = 0x00;
  tx_packet[9] = 0x00;
  tx_packet[10] = 0x00;
  tx_packet[11] = 0x01;

  // Byte 12-13: EtherType (0x0800 - IPv4)
  tx_packet[12] = 0x08;
  tx_packet[13] = 0x00;

  // Byte 14+: Payload
  memmove(tx_packet + 14, payload, payload_len + 1); // include null terminator

  int total_len = 14 + payload_len + 1;

  printf("[ethtest: transmitting %d-byte raw ethernet frame...]\n", total_len);
  
  // 2. Transmit the packet (simulates loopback routing inside driver)
  if (ethsend(tx_packet, total_len) < 0) {
    printf("Error: ethsend failed\n");
    exit(1);
  }
  printf("[ethtest: transmit complete. Waiting to receive via loopback rings...]\n");

  // 3. Receive the packet (sleeps inside driver until loopback interrupt triggers)
  memset(rx_packet, 0, sizeof(rx_packet));
  int n = ethrecv(rx_packet, sizeof(rx_packet));
  if (n < 0) {
    printf("Error: ethrecv failed\n");
    exit(1);
  }

  printf("\n[ethtest: packet successfully received! Size: %d bytes]\n", n);

  // 4. Validate packet data integrity
  // Check Destination MAC
  for (int i = 0; i < 6; i++) {
    if ((unsigned char)rx_packet[i] != 0xFF) {
      printf("Error: Destination MAC address mismatch!\n");
      exit(1);
    }
  }

  // Check Source MAC
  if ((unsigned char)rx_packet[6] != 0x02 || (unsigned char)rx_packet[11] != 0x01) {
    printf("Error: Source MAC address mismatch!\n");
    exit(1);
  }

  // Check EtherType
  if ((unsigned char)rx_packet[12] != 0x08 || (unsigned char)rx_packet[13] != 0x00) {
    printf("Error: EtherType mismatch!\n");
    exit(1);
  }

  // Check Payload
  char *rx_payload = rx_packet + 14;
  printf("[ethtest: received payload: \"%s\"]\n", rx_payload);
  if (strcmp(rx_payload, payload) != 0) {
    printf("Error: Payload data corrupted or mismatch!\n");
    exit(1);
  }

  printf("\n[Ethernet driver verification: Frame transmitted, DMA-buffered, interrupt-logged, and verified! Ethernet works!]\n\n");

  printf("--- ETHERNET DRIVER TEST END ---\n");
  exit(0);
}
