#ifndef INGREDIENT_BENCH_WIRE_H
#define INGREDIENT_BENCH_WIRE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { BENCH_SNAPSHOT=1, BENCH_ACK=2 };
typedef struct { uint8_t type, sequence, mask; bool valid; } BenchPacket;
typedef struct { uint8_t bytes[6], count; bool escape, discard; } BenchParser;
/* Bench-only protocol: 7E delimiter, escaped six-byte CRC8 packet, 7E.
 * Not compatible with the integrated game PeerLink protocol. */
size_t BenchWire_Encode(const BenchPacket *, uint8_t out[14]);
bool BenchWire_Receive(BenchParser *,uint8_t byte,BenchPacket *);
#endif
