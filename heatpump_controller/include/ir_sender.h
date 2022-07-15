#ifndef __IR_SENDER_H__
#define __IR_SENDER_H__

#include <mitsubishi_heatpump_codes.h>
#include <Arduino.h>

#define IR_HEADER_BIT_MARK MITSUBISHI_AIRCON1_HDR_MARK
#define IR_HEADER_SPACE MITSUBISHI_AIRCON1_HDR_SPACE
#define IR_BIT_MARK MITSUBISHI_AIRCON1_BIT_MARK
#define IR_ONE_SPACE MITSUBISHI_AIRCON1_ONE_SPACE
#define IR_ZERO_SPACE MITSUBISHI_AIRCON1_ZERO_SPACE
#define IR_INTER_MSG_SPACE MITSUBISHI_AIRCON1_MSG_SPACE

#define PIN_IR 6
#define IR_PWM_FREQUENCY 38 //kHz
#define IR_HALF_PERIOD 13 // 500 / IR_PWM_FREQUENCY
#define IR_SEND_REPEAT 2 // send data twice (header mark, header space, data bytes, mark, inter-message space, repeat header and data, mark, space(0))

void ir_bitbang();
void ir_mark(int len);
void ir_space(int len);
void ir_send_byte(uint8_t b);
void ir_send_message(uint8_t data[]);
void ir_mitsubishi_set_mode(uint8_t power, uint8_t mode, uint8_t *data);
void ir_mitsubishi_set_temperature(uint8_t temperature, uint8_t *data);
void ir_mitsubishi_set_fan(uint8_t fan, uint8_t *data);
void ir_mitsubishi_set_2flow(uint8_t flow, uint8_t *data);
void ir_mitsubishi_calculate_checksum(uint8_t *data);

#endif
