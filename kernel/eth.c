#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "eth.h"

// Static memory allocation for packet buffers
static char rx_buffers[ETH_RING_SIZE][ETH_MAX_PACKET];
static char tx_buffers[ETH_RING_SIZE][ETH_MAX_PACKET];

// Receive and Transmit rings representing hardware queues
static struct rx_desc rx_ring[ETH_RING_SIZE];
static struct tx_desc tx_ring[ETH_RING_SIZE];

// DMA ring index registers
static uint32 rx_head;      // OS Top-half consumer pointer (advances in ethreceive)
static uint32 rx_intr_head; // Interrupt bottom-half logging pointer (advances in ethintr)
static uint32 rx_tail;      // Next slot where hardware writes packets (advances in ethtransmit)
static uint32 tx_head;      // Next slot finished transmitting by hardware
static uint32 tx_tail;      // Next slot where driver writes packets for transmission

// Lock protecting ring buffer integrity
static struct spinlock eth_lock;

// Hexadecimal character digits helper
static char hex_digits[] = "0123456789abcdef";

// Output a single byte in padded hexadecimal format using raw console character writing
static void
printhex(unsigned char val)
{
  consputc(hex_digits[val >> 4]);
  consputc(hex_digits[val & 0xf]);
}

// Format and output a 6-byte hardware MAC address
static void
printmac(char *mac)
{
  for (int i = 0; i < 6; i++) {
    printhex((unsigned char)mac[i]);
    if (i < 5)
      consputc(':');
  }
}

// Initialize the Ethernet card rings and descriptors
void
ethinit(void)
{
  initlock(&eth_lock, "eth");

  acquire(&eth_lock);

  rx_head = 0;
  rx_intr_head = 0;
  rx_tail = ETH_RING_SIZE - 1;
  tx_head = 0;
  tx_tail = 0;

  // Set up RX ring (hardware buffers ready to be written to)
  // Set up TX ring (hardware buffers ready for transmission)
  for (int i = 0; i < ETH_RING_SIZE; i++) {
    rx_ring[i].addr = (uint64)rx_buffers[i];
    rx_ring[i].length = 0;
    rx_ring[i].status = 0; // ready for hardware receipt

    tx_ring[i].addr = (uint64)tx_buffers[i];
    tx_ring[i].length = 0;
    tx_ring[i].status = 0; // ready for driver writing
  }

  printf("eth: initialized simulated loopback Ethernet card (MAC 02:00:00:00:00:01)\n");

  release(&eth_lock);
}

// Transmit an Ethernet frame.
// Copies packet into a TX descriptor, loops it back directly into the next available RX slot
// to simulate hardware transmission over a loopback wire, then triggers a receive interrupt.
int
ethtransmit(char *buf, int len)
{
  if (len > ETH_MAX_PACKET || len < 0)
    return -1;

  acquire(&eth_lock);

  // Check if transmit ring is full
  uint32 next_tail = (tx_tail + 1) % ETH_RING_SIZE;
  if (next_tail == tx_head) {
    release(&eth_lock);
    return -1; // TX queue full, drop packet
  }

  // 1. Copy payload to current transmit buffer (DMA write simulator)
  memmove(tx_buffers[tx_tail], buf, len);
  tx_ring[tx_tail].length = len;
  tx_ring[tx_tail].status = 1; // marked active for transmission

  // 2. Hardware transmission loopback simulator:
  // Locate next empty receive ring slot where the card will "store" the incoming frame
  uint32 rx_slot = (rx_tail + 1) % ETH_RING_SIZE;
  if (rx_ring[rx_slot].status == 0) { // status 0 = descriptor is ready for card
    // Copy the packet directly to the RX descriptor's buffer
    memmove((void*)rx_ring[rx_slot].addr, tx_buffers[tx_tail], len);
    rx_ring[rx_slot].length = len;
    rx_ring[rx_slot].status = 1; // status 1 = packet received by hardware
    rx_tail = rx_slot;
  } else {
    // RX queue overrun: hardware drops the packet because RX descriptors are full
    printf("eth: RX queue overflow, dropping packet\n");
  }

  // 3. Complete transmission (free TX slot)
  tx_ring[tx_tail].status = 0;
  tx_tail = next_tail;

  release(&eth_lock);

  // 4. Trigger simulated hardware interrupt to process the packet
  ethintr();

  return 0;
}

// Simulated hardware interrupt handler (bottom-half of network driver).
// Processes received packets, hex-dumps them to the console using custom print format helpers,
// and wakes up waiting readers sleeping on the receive queue pointer.
void
ethintr(void)
{
  acquire(&eth_lock);

  // Process all packets marked as received (status == 1) that have not yet been printed
  while (rx_intr_head != (rx_tail + 1) % ETH_RING_SIZE) {
    uint32 slot = rx_intr_head;
    if (rx_ring[slot].status == 0) {
      break; // Slot not yet populated
    }

    char *packet = (char*)rx_ring[slot].addr;
    int len = rx_ring[slot].length;

    // Standard Ethernet Frame layout:
    // Bytes 0-5: Destination MAC
    // Bytes 6-11: Source MAC
    // Bytes 12-13: EtherType
    // Bytes 14+: Payload
    printf("\n--- [ETH INTR] Packet Received via Loopback ---\n");
    printf("Destination MAC: ");
    printmac(packet);
    printf("\nSource MAC:      ");
    printmac(packet + 6);
    printf("\nPacket Length:   %d bytes\n", len);

    // Print first 32 bytes of payload in hex using the zero-padded custom printer
    printf("Payload Hex:     ");
    int print_len = len > 32 ? 32 : len;
    for (int i = 14; i < print_len; i++) {
      printhex((unsigned char)packet[i]);
      consputc(' ');
    }
    if (len > 32)
      printf("...");
    printf("\n------------------------------------------------\n");

    // Wake up any thread waiting inside ethreceive()
    wakeup(&rx_head);

    // Advance the interrupt logging head pointer
    rx_intr_head = (rx_intr_head + 1) % ETH_RING_SIZE;
  }

  release(&eth_lock);
}

// Receive a packet from the RX descriptor ring (top-half of network driver).
// If no packet is available, it sleeps until ethintr() issues a wakeup.
int
ethreceive(char *buf, int max_len)
{
  acquire(&eth_lock);

  // Sleep until the current receive head is populated with a packet (status == 1)
  while (rx_ring[rx_head].status == 0) {
    if (killed(myproc())) {
      release(&eth_lock);
      return -1;
    }
    sleep(&rx_head, &eth_lock);
  }

  char *packet = (char*)rx_ring[rx_head].addr;
  int len = rx_ring[rx_head].length;
  if (len > max_len)
    len = max_len;

  // Copy the packet to user-space / destination buffer
  memmove(buf, packet, len);

  // Reclaim RX slot so the card can reuse it
  rx_ring[rx_head].status = 0;
  rx_head = (rx_head + 1) % ETH_RING_SIZE;

  release(&eth_lock);

  return len;
}
