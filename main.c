#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/ctrl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>

#include "pspdebug.h"

#define printf psvDebugScreenPrintf

#define PUP_PATH "ux0:app/FWUPDATER/PSP2UPDAT.PUP"

int sceSblSsUpdateMgrSetBootMode(int x);
int vshPowerRequestColdReset(void);

void ErrorExit(int milisecs, char *fmt, ...) {
  va_list list;
  char msg[256];

  va_start(list, fmt);
  vsprintf(msg, fmt, list);
  va_end(list);

  printf(msg);

  sceKernelDelayThread(milisecs * 1000);
  sceKernelExitProcess(0);
}

int get_pup_version(const char *pup, char *version) {
  int inf;

  if ((inf = sceIoOpen(pup, SCE_O_RDONLY, 0)) < 0) {
    return -1;
  }

  int ret = -1;
  int count;

  if (sceIoLseek(inf, 0x18, SCE_SEEK_SET) < 0) {
    goto end;
  }

  if (sceIoRead(inf, &count, 4) < 4) {
    goto end;
  }

  if (sceIoLseek(inf, 0x80, SCE_SEEK_SET) < 0) {
    goto end;
  }

  struct {
    uint64_t id;
    uint64_t off;
    uint64_t len;
    uint64_t field_18;
  } __attribute__((packed)) file_entry;

  for (int i = 0; i < count; i++) {
    if (sceIoRead(inf, &file_entry, sizeof(file_entry)) != sizeof(file_entry)) {
      goto end;
    }

    if (file_entry.id == 0x100) {
      break;
    }
  }

  if (file_entry.id == 0x100) {
    if (sceIoLseek(inf, file_entry.off, SCE_SEEK_SET) < 0) {
      goto end;
    }

    ret = sceIoRead(inf, version, file_entry.len);
  }

end:
  sceIoClose(inf);
  return ret;
}

int extract(const char *pup, const char *psp2swu) {
  int inf, outf;

  if ((inf = sceIoOpen(pup, SCE_O_RDONLY, 0)) < 0) {
    return -1;
  }

  if ((outf = sceIoOpen(psp2swu, SCE_O_CREAT | SCE_O_WRONLY | SCE_O_TRUNC, 6)) < 0) {
    return -1;
  }

  int ret = -1;
  int count;

  if (sceIoLseek(inf, 0x18, SCE_SEEK_SET) < 0) {
    goto end;
  }

  if (sceIoRead(inf, &count, 4) < 4) {
    goto end;
  }

  if (sceIoLseek(inf, 0x80, SCE_SEEK_SET) < 0) {
    goto end;
  }

  struct {
    uint64_t id;
    uint64_t off;
    uint64_t len;
    uint64_t field_18;
  } __attribute__((packed)) file_entry;

  for (int i = 0; i < count; i++) {
    if (sceIoRead(inf, &file_entry, sizeof(file_entry)) != sizeof(file_entry)) {
      goto end;
    }

    if (file_entry.id == 0x200) {
      break;
    }
  }

  if (file_entry.id == 0x200) {
    char buffer[1024];
    size_t rd;

    if (sceIoLseek(inf, file_entry.off, SCE_SEEK_SET) < 0) {
      goto end;
    }

    while (file_entry.len && (rd = sceIoRead(inf, buffer, sizeof(buffer))) > 0) {
      if (rd > file_entry.len) {
        rd = file_entry.len;
      }
      sceIoWrite(outf, buffer, rd);
      file_entry.len -= rd;
    }

    if (file_entry.len == 0) {
      ret = 0;
    }
  }

end:
  sceIoClose(inf);
  sceIoClose(outf);
  return ret;
}

int copy(const char *src, const char *dst) {
  SceUID fdsrc = sceIoOpen(src, SCE_O_RDONLY, 0);
  if (fdsrc < 0)
    return fdsrc;

  SceUID fddst = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6);
  if (fddst < 0) {
    sceIoClose(fdsrc);
    return fddst;
  }

  void *buf = memalign(4096, 64 * 1024);
  if (!buf) {
    sceIoClose(fddst);
    sceIoClose(fdsrc);
    return -1;
  }

  while (1) {
    int read = sceIoRead(fdsrc, buf, 64 * 1024);

    if (read < 0) {
      free(buf);

      sceIoClose(fddst);
      sceIoClose(fdsrc);

      sceIoRemove(dst);

      return read;
    }

    if (read == 0)
      break;

    int written = sceIoWrite(fddst, buf, read);

    if (written < 0) {
      free(buf);

      sceIoClose(fddst);
      sceIoClose(fdsrc);

      sceIoRemove(dst);

      return written;
    }
  }

  free(buf);

  sceIoClose(fddst);
  sceIoClose(fdsrc);

  return 0;
}

int main(int argc, char *argv[]) {
  SceCtrlData pad;
  int ret;

  psvDebugScreenInit();

  printf("Firmware Updater\n\n");

  printf("Press X to install PUP at %s.\n", PUP_PATH);
  printf("Press O to exit.\n\n");

  while (1) {
    sceCtrlPeekBufferPositive(0, &pad, 1);

    if (pad.buttons & SCE_CTRL_CROSS) {
          break;
    }

    if (pad.buttons & SCE_CTRL_CIRCLE) {
      ErrorExit(2000, "Exiting.\n");
    }

    sceKernelDelayThread(10 * 1000);
  }

  char version[8];
  ret = get_pup_version(PUP_PATH, version);
  if (ret < 0)
    ErrorExit(5000, "Error 0x%08X could not get version of PUP.\n", ret);

  char *p = strchr(version, '\x0A');
  if (p)
    *p = '\0';

  printf("PUP firmware %s detected.\n\n", version);

  printf("Are you sure you want to update?\n");
  printf("Press L+R+DOWN+X to update.\n");
  printf("Press O to cancel.\n\n");

  while (1) {
    sceCtrlPeekBufferPositive(0, &pad, 1);

    if (pad.buttons & SCE_CTRL_LTRIGGER &&
        pad.buttons & SCE_CTRL_RTRIGGER &&
        pad.buttons & SCE_CTRL_DOWN &&
        pad.buttons & SCE_CTRL_CROSS) {
          break;
    }

    if (pad.buttons & SCE_CTRL_CIRCLE) {
      ErrorExit(2000, "Exiting.\n");
    }

    sceKernelDelayThread(10 * 1000);
  }

  printf("Copying PUP to ud0:...\n");
  ret = copy(PUP_PATH, "ud0:PSP2UPDATE/PSP2UPDAT.PUP");
  if (ret < 0)
    ErrorExit(10000, "Error 0x%08X copying PSP2UPDAT.PUP.\n", ret);

  printf("Extracting updater...\n");
  ret = extract("ud0:PSP2UPDATE/PSP2UPDAT.PUP", "ud0:PSP2UPDATE/psp2swu.self");
  if (ret < 0)
    ErrorExit(10000, "Error 0x%08X extracting psp2swu.self.\n", ret);

  printf("Rebooting to update in 5 seconds...\n");
  sceKernelDelayThread(5 * 1000 * 1000);
  sceSblSsUpdateMgrSetBootMode(0x40);
  vshPowerRequestColdReset();

  return 0;
}
