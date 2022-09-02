cd psp_cex2dex
main.c
echo Enter your FuseID
echo to exit and save press escape, and type :wq
sleep 5
vim main.c :30
gcc main.c ids/aes_core.c -o cmac_gen
