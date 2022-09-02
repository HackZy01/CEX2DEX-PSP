cd psp_cex2dex
echo Enter your FuseID
echo to exit and save press escape, and type :wq
sleep 5
vim main.c :30
echo Now it's replace buf2 time, controls are the same
sleep 5
vim main.c :273
gcc main.c ids/aes_core.c -o cmac_gen
