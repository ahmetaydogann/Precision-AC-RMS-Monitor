#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <math.h>

#define RS PB0
#define EN PB1

// --- LCD ve ADC Fonksiyonları (Aynı) ---
void lcd_send_nibble(uint8_t n) {
    PORTC = (PORTC & 0xE1) | ((n & 0x0F) << 1); 
    PORTB |= (1 << EN); _delay_us(10); PORTB &= ~(1 << EN); _delay_us(100);
}
void lcd_command(uint8_t c) {
    PORTB &= ~(1 << RS);
    lcd_send_nibble(c >> 4); lcd_send_nibble(c & 0x0F); _delay_ms(2);
}
void lcd_data(uint8_t d) {
    PORTB |= (1 << RS);
    lcd_send_nibble(d >> 4); lcd_send_nibble(d & 0x0F); _delay_ms(2);
}
void lcd_print(const char* s) { while (*s) lcd_data(*s++); }
void lcd_init() {
    _delay_ms(50);
    lcd_send_nibble(0x03); _delay_ms(5);
    lcd_send_nibble(0x03); _delay_us(150);
    lcd_send_nibble(0x03); _delay_ms(1);
    lcd_send_nibble(0x02);
    lcd_command(0x28); lcd_command(0x0C); lcd_command(0x01); _delay_ms(2);
}
void adc_init() {
    ADMUX = (1 << REFS0); 
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); 
}
uint16_t adc_read() {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

int main(void) {
    DDRB |= (1 << RS) | (1 << EN);
    DDRC |= 0x1E;         
    DDRC &= ~(1 << DDC0); 

    lcd_init();
    adc_init();

    char buffer[16];
    
    // --- BÜYÜK SİLAH: AUTO-ZERO KALİBRASYONU ---
    lcd_command(0x01);
    lcd_print("KALIBRE EDILIYOR");
    long offset_toplam = 0;
    
    // Alternatörden gelen anlık veriyi sıfırlamak için 1000 örnek alıp ortalamasını buluyoruz
    for (int i = 0; i < 1000; i++) {
        offset_toplam += adc_read();
        _delay_us(100);
    }
    int dc_offset = offset_toplam / 1000; // Devrenin gerçek orta noktası artık bu!
    
    _delay_ms(500);

    while (1) {
        long sum_sq = 0;
        int samples = 500; 

        for (int i = 0; i < samples; i++) {
            int val = adc_read();
            // Sabit 512 veya 256 yerine, cihazın açılışta bulduğu ofseti çıkarıyoruz
            int ac_signal = val - dc_offset; 
            
            sum_sq += (long)ac_signal * ac_signal;
            _delay_us(500); 
        }

        float mean_sq = (float)sum_sq / samples;
        float rms_digital = sqrt(mean_sq);
        
        // Dirençsiz eski devremizdeki filtre kaybını tam kurtaran o efsane katsayı
        float v_rms = rms_digital * 0.00625;

        int tam = (int)v_rms;
        int ondalik = (int)((v_rms - tam) * 100);

        lcd_command(0x01); 
        lcd_print("AC RMS VALUE:");
        lcd_command(0xC0); 
        sprintf(buffer, "Vrms: %d.%02d Volt", tam, ondalik);
        lcd_print(buffer);

        _delay_ms(500); 
    }
}