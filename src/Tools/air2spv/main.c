// air2spv [-q] [-Q] [-n] [-D] [-s SET] [-p PCBASE] IN.metallib -o OUT.spv [-r OUT.json]: one AIR function to SPIR-V
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "air2spv.h"

static uint8_t *slurp(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  uint8_t *d;
  long n;

  if (!f)
    return 0;

  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  d = malloc((size_t)n + 1);
  if (fread(d, 1, (size_t)n, f) != (size_t)n) {
    fclose(f);
    free(d);
    return 0;
  }
  fclose(f);
  *len = (size_t)n;
  return d;
}

int main(int argc, char **argv) {
  struct a2s_options opt = { 1, 0, 0, 0, 0, 0, 0, 0 };
  struct a2s_reflect refl;
  const char *in = 0, *out = 0, *rj = 0, *vin = 0;
  struct a2s_pin pins[8];
  unsigned npins = 0;
  uint32_t *words;
  size_t len, nw;
  uint8_t *data;
  char err[512];
  FILE *f;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-o") && i + 1 < argc)
      out = argv[++i];
    else if (!strcmp(argv[i], "-r") && i + 1 < argc)
      rj = argv[++i];
    else if (!strcmp(argv[i], "-s") && i + 1 < argc)
      opt.set = (uint32_t)atoi(argv[++i]);
    else if (!strcmp(argv[i], "-p") && i + 1 < argc)
      opt.pc_base = (uint32_t)atoi(argv[++i]);
    else if (!strcmp(argv[i], "-q"))
      opt.quantize_half = 0;
    else if (!strcmp(argv[i], "-Q"))
      opt.quad_memory = 1;
    else if (!strcmp(argv[i], "-n"))
      opt.narrow_loads = 1;
    else if (!strcmp(argv[i], "-D"))
      opt.no_dynamic_index = 1;
    else if (!strcmp(argv[i], "-K"))
      opt.kill_discard = 1;
    else if (!strcmp(argv[i], "-V") && i + 1 < argc)
      vin = argv[++i];
    else if (!strcmp(argv[i], "-P") && i + 1 < argc && npins < 8 && strchr(argv[i + 1], ':')) {
      // -P METAL:FILE: constant buffer METAL holds FILE's bytes for this pipeline
      size_t pl;
      char *a = argv[++i];

      pins[npins].metal = (uint32_t)atoi(a);
      pins[npins].data = slurp(strchr(a, ':') + 1, &pl);
      pins[npins].size = (uint32_t)pl;
      if (pins[npins].data)
        npins++;
    }
    else
      in = argv[i];
  }

  opt.pins = pins, opt.npins = npins;
  if (!in) {
    fprintf(stderr, "usage: air2spv [-q] [-Q] [-n] [-D] [-K] [-s SET] [-p PCBASE] [-V VERT.metallib] [-P METAL:FILE] IN.metallib [-o OUT.spv] [-r OUT.json]\n");
    return 2;
  }

  if (!(data = slurp(in, &len))) {
    fprintf(stderr, "air2spv: cannot read %s\n", in);
    return 2;
  }

  if (a2s_translate(data, (uint32_t)len, &opt, &words, &nw, &refl, err, sizeof(err))) {
    fprintf(stderr, "air2spv: %s: %s\n", in, err);
    return 1;
  }

  // -V: the fragment inputs moved to the given vertex shader's output locations, as airjitd links a pipeline.
  // the vertex shader (set 0, no push constant base) goes to OUT.vert.spv and OUT.vert.json
  if (vin && out) {
    struct a2s_options vo = opt;
    struct a2s_reflect vr;
    uint32_t *vw;
    size_t vlen, vn;
    uint8_t *vd = slurp(vin, &vlen);
    char path[1024];

    vo.set = 0, vo.pc_base = 0;
    if (!vd || a2s_translate(vd, (uint32_t)vlen, &vo, &vw, &vn, &vr, err, sizeof(err))) {
      fprintf(stderr, "air2spv: %s: %s\n", vin, vd ? err : "cannot read");
      return 1;
    }
    fprintf(stderr, "air2spv: %u of %u inputs matched\n", a2s_relocate(words, &refl, &vr), refl.nvar);
    snprintf(path, sizeof(path), "%s.vert.spv", out);
    if ((f = fopen(path, "wb"))) {
      fwrite(vw, 4, vn, f);
      fclose(f);
    }
    snprintf(path, sizeof(path), "%s.vert.json", out);
    if ((f = fopen(path, "w"))) {
      a2s_reflect_json(&vr, f);
      fclose(f);
    }
    free(vw);
    free(vd);
  }

  if (out && (f = fopen(out, "wb"))) {
    fwrite(words, 4, nw, f);
    fclose(f);
  }

  if (rj && (f = fopen(rj, "w"))) {
    a2s_reflect_json(&refl, f);
    fclose(f);
  } else if (!out) {
    a2s_reflect_json(&refl, stdout);
  }

  free(words);
  free(data);
  return 0;
}
