# sftool 烧录指南

`<PORT>` 为串口号，如 `COM3`。

## 烧录 ELF 文件

ELF 文件包含地址信息，不需要手动指定 `@地址`：

```powershell
.\sftool -c SF32LB52 -p <PORT> write_flash firmware.elf
```

## 烧录 BIN 文件

BIN 文件不含地址信息，需要用 `@地址` 指定烧录起始地址：

```powershell
.\sftool -c SF32LB52 -p <PORT> write_flash firmware.bin@0x12020000
```

## 一次烧录多个文件

多个文件可以在同一条命令中依次列出，每个文件各自指定地址：

```powershell
.\sftool -c SF32LB52 -p <PORT> write_flash bootloader.bin@0x12000000 firmware.bin@0x12020000
```
