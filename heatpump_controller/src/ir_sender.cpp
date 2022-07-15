#include <ir_sender.h>

void ir_bitbang()
{
    digitalWrite(PIN_IR, HIGH);
    delayMicroseconds(IR_HALF_PERIOD);
    digitalWrite(PIN_IR, LOW);
    delayMicroseconds(IR_HALF_PERIOD);
}

void ir_mark(int len)
{
    long start = micros();
    while ((int)(micros() - start) < len) {
        digitalWrite(PIN_IR, HIGH);
        delayMicroseconds(IR_HALF_PERIOD);
        digitalWrite(PIN_IR, LOW);
        delayMicroseconds(IR_HALF_PERIOD);
    }
}

void ir_space(int len)
{
    digitalWrite(PIN_IR, LOW);
    if (len < 16383) {
        delayMicroseconds(len);
    } else {
        delay(len / 1000);
    }
}

void ir_send_byte(uint8_t b)
{
    for (int i=0; i<8 ; i++)
    {
        if (b & 0x01)
        {
            ir_mark(IR_BIT_MARK);
            ir_space(IR_ONE_SPACE);
        }
        else
        {
            ir_mark(IR_BIT_MARK);
            ir_space(IR_ZERO_SPACE);
        }
        b >>= 1;
    }
}

void ir_send_message(uint8_t data[]) 
{
    digitalWrite(13, HIGH);

    for (int repeats = 0; repeats < 2; repeats++)
    {
        ir_mark(IR_HEADER_BIT_MARK);
        ir_space(IR_HEADER_SPACE);
        for (uint8_t data_i = 0; data_i < 18 /* ? */; data_i++)
        {
            ir_send_byte(data[data_i]);
        }
        if (repeats == 0)
        {
            ir_mark(IR_BIT_MARK);
            ir_space(IR_INTER_MSG_SPACE);
        }
    }
    ir_mark(IR_BIT_MARK);
    ir_space(0); // turn transmitter off

    digitalWrite(13, LOW);
}

void ir_mitsubishi_set_mode(uint8_t power, uint8_t mode, uint8_t *data)
{
    data[5] = power == 0 ? MITSUBISHI_AIRCON1_MODE_OFF : MITSUBISHI_AIRCON1_MODE_ON;
    data[6] = mode == 0 ? MITSUBISHI_AIRCON1_MODE_HEAT : MITSUBISHI_AIRCON1_MODE_COOL;
}

void ir_mitsubishi_set_temperature(uint8_t temperature, uint8_t *data)
{
    data[7] = temperature - 16;
}

void ir_mitsubishi_set_fan(uint8_t fan, uint8_t *data)
{
    data[9] = fan;
}

void ir_mitsubishi_set_2flow(uint8_t flow, uint8_t *data)
{
    data[16] = (flow == 2) ? 0 : 2;
}

void ir_mitsubishi_calculate_checksum(uint8_t *data)
{
    uint8_t cs = 0;
    for (int i = 0; i < 17; i++)
    {
        cs += data[i];
    }
    data[17] = cs;
}
