echo edit line 30 in xxx/CEX2DEX-PSP/psp_cex2dex/main.c with your own fuseid and press Y to continue
if [ "$input" != "Y" ] && [ "$input" != "y" ]; then
cd psp_cex2dex
gcc main.c ids/aes_core.c -o cmac_gen
