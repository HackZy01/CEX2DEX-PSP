#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ids/aes.h"

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct
{
	u8  buf1[8]; // 0
	u8  buf2[8]; // 8
	u8  buf3[8]; // 0x10
} SomeStructure;

u8 idskey0[16] = 
{
	0x47, 0x5E, 0x09, 0xF4, 0xA2, 0x37, 0xDA, 0x9B, 
	0xEF, 0xFF, 0x3B, 0xC0, 0x77, 0x14, 0x3D, 0x8A
};

typedef struct U64
{
	u32 low;
	u32 high;
} U64;

//change this fuseid to your own
u_int64_t g_fuseid = 0x570804b80a0f;

int CreateSS(SomeStructure *ss)
{
	U64 fuse;
	int i;

	memcpy(&fuse, &g_fuseid, 8);
	
	memset(ss->buf1, 0, 8);
	memset(ss->buf2, 0xFF, 8);

	memcpy(ss->buf3, &fuse.high, 4);
	memcpy(ss->buf3+4, &fuse.low, 4);

	for (i = 0; i < 4; i++)
	{
		ss->buf1[3-i] = ss->buf3[i];
		ss->buf1[7-i] = ss->buf3[4+i];
	}

	return 0;
}

void P1(u8 *buf)
{
	u8 m[0x40]; // sp
	int i, j;
	char rr[5] = { 0x01, 0x0F, 0x36, 0x78, 0x40 }; // sp+0x50
	u8 ss[5]; // sp+0x40
	
	memset(m, 0, 0x40);
	memcpy(m, buf, 0x3C);

	for (i = 0; i < 0x3C; i++)
	{
		for (j = 0; j < 5; j++)
		{
			u8 x;
			u32 y;
			char c;

			x = 0;			
			c = rr[j];
			y = m[i];
			
			while (c != 0)
			{
				if (c & 1)
				{
					x ^= y;
				}

				y = (y << 1);
					
				if ((y & 0x0100))
				{
					y ^= 0x11D;
				}

				c /= 2;
			}

			ss[j] = x;
		}

		for (j = 0; j < 5; j++)
		{
			m[i+j] ^= ss[j]; 
		}
	}

	for (i = 0x3C; i < 0x40; i++)
	{
		buf[i] = m[i];
		m[i] = 0;
	}
}

void GenerateSigncheck(SomeStructure *ss, int *b, u8 *out)
{
	AES_KEY ctx, ctx2; // sp+0x20
	int i, j;
	u8 sg_key1[0x10], sg_key2[0x10]; // sp, sp+0x10
		
	AES_set_encrypt_key(idskey0, 128, &ctx);
	AES_set_decrypt_key(idskey0, 128, &ctx2);

	for (i = 0; i < 16; i++)
	{
		sg_key1[i] = sg_key2[i] = ss->buf1[i % 8];
	}

	for (i = 0; i < 3; i++)
	{
		AES_encrypt_ids(sg_key2, sg_key2, &ctx);
		AES_decrypt_ids(sg_key1, sg_key1, &ctx2);
	}

	AES_set_encrypt_key(sg_key2, 128, &ctx);

	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			AES_encrypt_ids(sg_key1, sg_key1, &ctx);
		}

		memcpy(out+(i*16), sg_key1, 0x10);		
	}

	memcpy(out+0x30, ss->buf1, 8);
	memcpy(out+0x38, b, 4);

	P1(out);
}

int EncryptRegion(u8 *scheck, u8 *in, u32 size, u8 *out)
{
	const int n = 3; // param t0
	int a;
	int i, j;
	u8  k1[0x10], k2[0x10]; // sp, sp+0x110
	u8 *r;
	AES_KEY ctx; // (fp=sp+0x10)
	int v = size / 16;
	
	a = (size & 0x0F);

	if (a != 0 || (size == 0 && (n & 1)))
	{
		if ((n & 2) == 0)
			return -1;
		
		v++;
		r = k1;
		
		for (i = 0; i < a; i++)
		{
			k1[i] = in[size-a+i];
		}

		k1[i++] = 0x80;

		for (; i < 0x10; i++)
		{
			k1[i] = 0;
		}

		a = 0x10;
	}
	else
	{
		r = (in+(v*16))-0x10;
	}

	AES_set_encrypt_key(scheck, 128, &ctx);

	if ((n & 1))
	{
		memset(out, 0, 16);
	}
	else if (v > 0)
	{
		AES_encrypt_ids(out, out, &ctx);
	}

	for (i = 0; i < (v-1); i++)
	{
		for (j = 0; j < 16; j++)
		{
			out[j] ^= in[i*16+j];
		}

		AES_encrypt_ids(out, out, &ctx);
	}	

	if (v > 0)
	{
		for (i = 0; i < 0x10; i++)
		{
			out[i] ^= r[i];
		}
	}

	if (!(n & 2))
	{
		return 0;
	}

	memset(k2, 0, 0x10);
	AES_encrypt_ids(k2, k2, &ctx);

	a = a ? 2 : 1;
	
	for (i = 0; i < a; i++)
	{
		u32 x = ((char)k2[0] < 0);
		
		for (j = 0; j < 15; j++)
		{
			u32 t = ((k2[j+1] >> 7) | (k2[j] << 1));
			k2[j] = t;
		}

		k2[15] <<= 1;

		if (x == 1)
		{
			int t = (int)k2[15] ^ 0xFFFFFF87;
			k2[15] = t;
		}
	}

	for (i = 0; i < 0x10; i++)
	{
		out[i] ^= k2[i];		
	}

	AES_encrypt_ids(out, out, &ctx);

	return 0;
}

void hexDump(const void *data, size_t size) {
  size_t i;
  for (i = 0; i < size; i++) {
    printf("%02hhX%c", ((char *)data)[i], (i + 1) % 16 ? ' ' : '\n');
  }
  printf("\n");
}

int main (){
	int i;
	
	SomeStructure ss;

	CreateSS(&ss);
	
	
	// change this certificate to your own, remember that the target id is the 6th byte, change it to 0x02 for dex
	// phat 1000 can install dex ofw and cfw m33
	// brite 3000 can only install cfw m33
	u_int8_t buf2[0xA8] = {
		0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x01, 0x04, 0x0B, 0x2F, 0x84, 0xA9, 0x69, 0xEA, 0xEE, 0x7E, 0x1B, 0x7D, 0x83, 0x56, 0xCE, 0xFA, 0xEE, 0xB7, 0xB9, 0xC1, 0x02, 0x7F, 0x7E, 0x21, 0x26, 0x9E, 0xEB, 0x3E, 0xF9, 0x1B, 0x2A, 0x6B, 0x33, 0xC0, 0xFF, 0xC8, 0x42, 0x50, 0x0F, 0xAE, 0xA3, 0x1F, 0xF6, 0x7A, 0x7A, 0x89, 0x46, 0xEB, 0xED, 0xBF, 0x88, 0x1C, 0xF4, 0x0C, 0xFA, 0xE3, 0xFC, 0x66, 0x6C, 0x7D, 0x64, 0x18, 0xE7, 0xA6, 0x1A, 0x54, 0xFD, 0xD0, 0xC9, 0x2B, 0x83, 0x54, 0xD4, 0x9E, 0xA5, 0x38, 0x80, 0xE0, 0x7E, 0xAF, 0x29, 0x0B, 0x03, 0x3E, 0x7E, 0x12, 0xFD, 0xDC, 0xDD, 0x40, 0x04, 0xC8, 0x0B, 0xD9, 0xC8, 0xBA, 0x38, 0x22, 0x10, 0x65, 0x92, 0x3E, 0x32, 0x4B, 0x5F, 0x0E, 0xC1, 0x65, 0xED, 0x6C, 0xFF, 0x7D, 0x9F, 0x2C, 0x42, 0x0B, 0x84, 0xDF, 0xDA, 0x6E, 0x96, 0xC0, 0xAE, 0xE2, 0x99, 0x27, 0xBC, 0xAF, 0x1E, 0x39, 0x3C, 0xB5, 0x6F, 0x54, 0x18, 0x60, 0x36, 0xCB, 0x58, 0x8D, 0xAB, 0xFE, 0x83, 0x38, 0xB6, 0xD2, 0x89, 0x4E, 0x50, 0xCD, 0x0F, 0xCF, 0x71, 0xEA, 0xF0, 0x03, 0xAF, 0x1B, 0x6C, 0xF9, 0xE8
	};
	
	u_int8_t buf3[0x10];
	
	int b=0;
	u32 c[0x40/4];
	
	AES_KEY ctx;
	
	GenerateSigncheck(&ss, &b, (void *)c);
	
	AES_set_encrypt_key((void *)(c+8), 128, &ctx);
	
	for (i = 0; i < 2; i++)
	{
		AES_encrypt_ids((void *)(c+4), (void *)(c+4), &ctx);
	}

	for (i = 0; i < 3; i++)
	{
		AES_encrypt_ids((u8 *)c, (u8 *)c, &ctx);
	}
	
	EncryptRegion((void *)c,buf2,0xA8,buf3);
	
	u_int8_t certificate_final[0xB8];
	
	memcpy(certificate_final, buf2, 0xA8);
	
	memcpy(certificate_final + 0xA8, buf3, 0x10);
	
	printf("final certificate is : \n");
	
	hexDump(certificate_final, 0xB8);
}