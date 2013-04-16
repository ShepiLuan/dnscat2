#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "buffer.h"
#include "memory.h"
#include "packet.h"

packet_t *packet_parse(uint8_t *data, size_t length)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));
  buffer_t *buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

  /* Validate the size */
  if(buffer_get_length(buffer) > MAX_PACKET_SIZE)
  {
    printf("Packet is too long: %zu bytes\n", buffer_get_length(buffer));
    exit(1);
  }

  packet->message_type = buffer_read_next_int8(buffer);
  packet->packet_id    = buffer_read_next_int16(buffer);
  packet->session_id   = buffer_read_next_int16(buffer);

  switch(packet->message_type)
  {
    case MESSAGE_TYPE_SYN:
      packet->body.syn.seq     = buffer_read_next_int16(buffer);
      packet->body.syn.options = buffer_read_next_int16(buffer);
      break;

    case MESSAGE_TYPE_MSG:
      packet->body.msg.seq     = buffer_read_next_int16(buffer);
      packet->body.msg.ack     = buffer_read_next_int16(buffer);
      packet->body.msg.data    = buffer_read_remaining_bytes(buffer, &packet->body.msg.data_length, -1, FALSE);
      break;

    case MESSAGE_TYPE_FIN:
      /* Do nothing */
      break;

    default:
      printf("Error: unknown message type (0x%02x)\n", packet->message_type);
      exit(0);
  }

  buffer_destroy(buffer);

  return packet;
}

packet_t *packet_create_syn(uint16_t packet_id, uint16_t session_id, uint16_t seq, uint16_t options)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));
  packet->message_type     = MESSAGE_TYPE_SYN;
  packet->packet_id        = packet_id;
  packet->session_id       = session_id;
  packet->body.syn.seq     = seq;
  packet->body.syn.options = options;

  return packet;
}

packet_t *packet_create_msg(uint16_t packet_id, uint16_t session_id, uint16_t seq, uint16_t ack, uint8_t *data, size_t data_length)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->message_type         = MESSAGE_TYPE_MSG;
  packet->packet_id            = packet_id;
  packet->session_id           = session_id;
  packet->body.msg.seq         = seq;
  packet->body.msg.ack         = ack;
  packet->body.msg.data        = safe_memcpy(data, data_length);
  packet->body.msg.data_length = data_length;

  return packet;
}

packet_t *packet_create_fin(uint16_t packet_id, uint16_t session_id)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->message_type     = MESSAGE_TYPE_FIN;
  packet->packet_id        = packet_id;
  packet->session_id       = session_id;

  return packet;
}

uint8_t *packet_to_bytes(packet_t *packet, size_t *length)
{
  buffer_t *buffer = buffer_create(BO_BIG_ENDIAN);

  buffer_add_int8(buffer, packet->message_type);
  buffer_add_int16(buffer, packet->packet_id);
  buffer_add_int16(buffer, packet->session_id);

  switch(packet->message_type)
  {
    case MESSAGE_TYPE_SYN:
      buffer_add_int16(buffer, packet->body.syn.seq);
      buffer_add_int16(buffer, packet->body.syn.options);
      break;

    case MESSAGE_TYPE_MSG:
      buffer_add_int16(buffer, packet->body.msg.seq);
      buffer_add_int16(buffer, packet->body.msg.ack);
      buffer_add_bytes(buffer, packet->body.msg.data, packet->body.msg.data_length);
      break;

    case MESSAGE_TYPE_FIN:
      /* Do nothing */
      break;

    default:
      printf("Error: Unknown message type: %u\n", packet->message_type);
  }

  return buffer_create_string_and_destroy(buffer, length);
}

void packet_print(packet_t *packet)
{
  if(packet->message_type == MESSAGE_TYPE_SYN)
    printf("[[SYN]] :: packet_id = %04x, session = %04x, seq = %04x, options = %04x\n", packet->packet_id, packet->session_id, packet->body.syn.seq, packet->body.syn.options);
  else if(packet->message_type == MESSAGE_TYPE_MSG)
    printf("[[MSG]] :: packet_id = %04x, session = %04x, seq = %04x, ack = %04x, data = \"%s\"\n", packet->packet_id, packet->session_id, packet->body.msg.seq, packet->body.msg.ack, packet->body.msg.data);
  else if(packet->message_type == MESSAGE_TYPE_FIN)
    printf("[[FIN]] :: packet_id = %04x, session = %04x\n", packet->packet_id, packet->session_id);
  else
  {
    printf("Unknown packet type!\n");
    exit(1);
  }
}

void packet_destroy(packet_t *packet)
{
  if(packet->message_type == MESSAGE_TYPE_MSG)
    safe_free(packet->body.msg.data);

  safe_free(packet);
}

