#ifndef _ETH_H_
#define _ETH_H_

#define ETH_RING_SIZE   16
#define ETH_MAX_PACKET  1518

// Receive ring descriptor
struct rx_desc {
  uint64 addr;    // Virtual address of the buffer
  uint32 length;  // Length of packet in bytes
  uint32 status;  // Status flag: 0 = free/ready for HW, 1 = packet received
};

// Transmit ring descriptor
struct tx_desc {
  uint64 addr;    // Virtual address of the buffer
  uint32 length;  // Length of packet in bytes
  uint32 status;  // Status flag: 0 = ready for driver, 1 = marked for transmission
};

#endif
