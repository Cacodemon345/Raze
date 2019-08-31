
#ifndef __light_h__
#define __light_h__

void MyLoadPalette();
int LoadPaletteLookups();
void WaitVBL();
void SetGreenPal();
void RestoreGreenPal();
void FixPalette();
void FadeToWhite();

extern void DoOverscanSet(short someval);
void SetOverscan(unsigned char *palette);

extern unsigned char kenpal[];
extern short overscanindex;

extern char *origpalookup[];

extern short nPalDiff;

#endif