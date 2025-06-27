#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#define I2C_ADDR 0x27  // Endereço I2C do LCD (use i2cdetect para confirmar)

#define LCD_BACKLIGHT 0x08
#define ENABLE 0b00000100
#define LCD_CHR 1
#define LCD_CMD 0

int fd;

void lcd_toggle_enable(uint8_t bits) {
    write(fd, &bits, 1);
    usleep(500);

    uint8_t enableBits = bits | ENABLE;
    write(fd, &enableBits, 1);
    usleep(500);

    enableBits = bits & ~ENABLE;
    write(fd, &enableBits, 1);
    usleep(500);
}

void lcd_byte(uint8_t bits, uint8_t mode) {
    uint8_t high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    uint8_t low  = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    write(fd, &high, 1);
    lcd_toggle_enable(high);

    write(fd, &low, 1);
    lcd_toggle_enable(low);
}

void lcd_init() {
    lcd_byte(0x33, LCD_CMD); // Inicializa em 8-bit
    lcd_byte(0x32, LCD_CMD); // Força modo 4-bit
    lcd_byte(0x06, LCD_CMD); // Modo de entrada
    lcd_byte(0x0C, LCD_CMD); // Display on, cursor off
    lcd_byte(0x28, LCD_CMD); // 2 linhas, 5x8 matriz
    lcd_byte(0x01, LCD_CMD); // Limpa display
    usleep(2000);
}

void lcd_set_cursor(int line) {
    lcd_byte(line == 1 ? 0xC0 : 0x80, LCD_CMD);
}

void lcd_print(const std::string& text) {
    for (char c : text) {
        lcd_byte(c, LCD_CHR);
    }
}

int main() {
    const char *i2c_file = "/dev/i2c-1";
    fd = open(i2c_file, O_RDWR);
    if (fd < 0) {
        std::cerr << "Erro ao abrir o barramento I2C\n";
        return 1;
    }

    if (ioctl(fd, I2C_SLAVE, I2C_ADDR) < 0) {
        std::cerr << "Erro ao conectar com o LCD no endereço 0x27\n";
        return 1;
    }

    lcd_init();

    lcd_set_cursor(0);
    lcd_print("Ola Raspberry!");

    lcd_set_cursor(1);
    lcd_print("LCD I2C ok!");

    close(fd);
    return 0;
}
