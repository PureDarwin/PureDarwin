#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a2s.h"

// a phi's function variable lives under its value with the low bit set
static LLVMValueRef phi_key(LLVMValueRef phi) {
  return (LLVMValueRef)((uintptr_t)phi | 1);
}

static int block_index(struct a2s *a, LLVMBasicBlockRef bb) {
  for (int i = 0; i < a->nb; i++) {
    if (a->b[i].bb == bb)
      return i;
  }

  a2s_fail(a, "branch to a block outside the function");
}

static int in_body(struct a2s *a, int h, int x) {
  return x >= 0 && x < a->nb && (a->b[h].body[x / 64] >> (x % 64) & 1);
}

static int dominates(struct a2s *a, int x, int y) {
  while (y >= 0) {
    if (y == x)
      return 1;
    if (a->b[y].idom == y)
      return 0;
    y = a->b[y].idom;
  }

  return 0;
}

// dominators by Cooper, Harvey and Kennedy over a graph given as successor lists, n is the root
static void dom_tree(int nn, int root, int **succ, int *nsucc, int *idom) {
  int *order = malloc(sizeof(int) * (size_t)nn), *num = malloc(sizeof(int) * (size_t)nn);
  int *stack = malloc(sizeof(int) * (size_t)nn * 2), *it = calloc((size_t)nn, sizeof(int));
  int **pred = calloc((size_t)nn, sizeof(int *)), *npred = calloc((size_t)nn, sizeof(int));
  int no = 0, sp = 0, changed = 1, i, k;

  for (i = 0; i < nn; i++) {
    num[i] = -1, idom[i] = -1;
    for (k = 0; k < nsucc[i]; k++) {
      int s = succ[i][k];
      pred[s] = realloc(pred[s], sizeof(int) * (size_t)(npred[s] + 1));
      pred[s][npred[s]++] = i;
    }
  }

  // postorder by an explicit stack
  stack[sp++] = root, num[root] = -2;
  while (sp) {
    int u = stack[sp - 1];
    if (it[u] < nsucc[u]) {
      int v = succ[u][it[u]++];
      if (num[v] == -1)
        num[v] = -2, stack[sp++] = v;
    } else {
      num[u] = no, order[no++] = u, sp--;
    }
  }

  idom[root] = root;
  while (changed) {
    changed = 0;
    for (k = no - 2; k >= 0; k--) {
      int u = order[k], nd = -1;
      for (i = 0; i < npred[u]; i++) {
        int p = pred[u][i], x, y;
        if (num[p] < 0 || idom[p] < 0)
          continue;
        if (nd < 0) {
          nd = p;
          continue;
        }
        x = p, y = nd;
        while (x != y) {
          while (num[x] < num[y]) {
            x = idom[x];
          }
          while (num[y] < num[x]) {
            y = idom[y];
          }
        }
        nd = x;
      }
      if (nd >= 0 && idom[u] != nd)
        idom[u] = nd, changed = 1;
    }
  }

  for (i = 0; i < nn; i++) {
    free(pred[i]);
  }
  free(pred), free(npred), free(order), free(num), free(stack), free(it);
}

static void build(struct a2s *a) {
  unsigned n = LLVMCountBasicBlocks(a->fn);
  LLVMBasicBlockRef *bbs = malloc(sizeof(*bbs) * n);
  int **succ, *nsucc, *idom, i, k, words;

  LLVMGetBasicBlocks(a->fn, bbs);
  a->nb = (int)n;
  a->b = calloc(n, sizeof(*a->b));
  words = (a->nb + 63) / 64;
  a->nwords = words;

  for (i = 0; i < a->nb; i++) {
    a->b[i].bb = bbs[i];
    a->b[i].label = spv_id(&a->s);
    a->b[i].exit = -1;
  }

  for (i = 0; i < a->nb; i++) {
    LLVMValueRef t = LLVMGetBasicBlockTerminator(bbs[i]);
    unsigned ns = t ? LLVMGetNumSuccessors(t) : 0;

    a->b[i].succ = malloc(sizeof(int) * (ns ? ns : 1));
    for (k = 0; k < (int)ns; k++) {
      a->b[i].succ[k] = block_index(a, LLVMGetSuccessor(t, (unsigned)k));
    }
    a->b[i].nsucc = (int)ns;
  }

  // dominators from the entry, post dominators from a virtual exit every exiting block feeds
  succ = malloc(sizeof(int *) * (size_t)(n + 1));
  nsucc = calloc(n + 1, sizeof(int));
  idom = malloc(sizeof(int) * (n + 1));
  for (i = 0; i < a->nb; i++) {
    succ[i] = a->b[i].succ, nsucc[i] = a->b[i].nsucc;
  }
  succ[n] = 0;
  dom_tree(a->nb, 0, succ, nsucc, idom);
  for (i = 0; i < a->nb; i++) {
    a->b[i].idom = idom[i];
    a->b[i].reach = idom[i] >= 0;
  }
  for (i = 0; i < a->nb; i++) {
    for (k = 0; a->b[i].reach && k < a->b[i].nsucc; k++) {
      a->b[a->b[i].succ[k]].npred++;
    }
  }

  for (i = 0; i <= a->nb; i++) {
    succ[i] = 0, nsucc[i] = 0;
  }
  for (i = 0; i < a->nb; i++) {
    if (!a->b[i].reach)
      continue;
    if (!a->b[i].nsucc) {
      succ[n] = realloc(succ[n], sizeof(int) * (size_t)(nsucc[n] + 1));
      succ[n][nsucc[n]++] = i;
    }
    for (k = 0; k < a->b[i].nsucc; k++) {
      int s = a->b[i].succ[k];
      succ[s] = realloc(succ[s], sizeof(int) * (size_t)(nsucc[s] + 1));
      succ[s][nsucc[s]++] = i;
    }
  }
  dom_tree(a->nb + 1, a->nb, succ, nsucc, idom);
  for (i = 0; i < a->nb; i++) {
    a->b[i].ipdom = idom[i] == a->nb ? -1 : idom[i];
  }
  for (i = 0; i <= a->nb; i++) {
    free(succ[i]);
  }
  free(succ), free(nsucc), free(idom), free(bbs);

  // natural loops: a back edge u -> h where h dominates u
  for (i = 0; i < a->nb; i++) {
    if (!a->b[i].reach)
      continue;
    for (k = 0; k < a->b[i].nsucc; k++) {
      int h = a->b[i].succ[k], *st, sp = 0;

      if (!dominates(a, h, i))
        continue;

      if (!a->b[h].header) {
        a->b[h].header = 1;
        a->b[h].body = calloc((size_t)words, 8);
        a->b[h].body[h / 64] |= 1ull << (h % 64);
      }

      st = malloc(sizeof(int) * (size_t)a->nb);
      if (!in_body(a, h, i)) {
        a->b[h].body[i / 64] |= 1ull << (i % 64);
        st[sp++] = i;
      }
      while (sp) {
        int u = st[--sp];
        for (int p = 0; p < a->nb; p++) {
          if (!a->b[p].reach || in_body(a, h, p))
            continue;
          for (int q = 0; q < a->b[p].nsucc; q++) {
            if (a->b[p].succ[q] == u) {
              a->b[h].body[p / 64] |= 1ull << (p % 64);
              st[sp++] = p;
              break;
            }
          }
        }
      }
      free(st);
    }
  }

  // a loop's one exit: the target outside its body of every edge leaving it
  for (i = 0; i < a->nb; i++) {
    if (!a->b[i].header)
      continue;
    for (int u = 0; u < a->nb; u++) {
      if (!in_body(a, i, u))
        continue;
      for (k = 0; k < a->b[u].nsucc; k++) {
        int s = a->b[u].succ[k];
        if (in_body(a, i, s))
          continue;
        if (a->b[i].exit >= 0 && a->b[i].exit != s)
          a2s_fail(a, "loop with more than one exit");
        a->b[i].exit = s;
      }
    }
  }
}

static void phi_vars(struct a2s *a) {
  for (int i = 0; i < a->nb; i++) {
    for (LLVMValueRef in = LLVMGetFirstInstruction(a->b[i].bb); in && LLVMIsAPHINode(in); in = LLVMGetNextInstruction(in)) {
      LLVMTypeRef t = LLVMTypeOf(in);
      uint32_t ty = LLVMGetTypeKind(t) == LLVMPointerTypeKind ? spv_t_uint(&a->s) : a2s_type(a, t), v = spv_id(&a->s);
      struct vslot *s;

      spv_op(&a->vars, SpvOpVariable, 3, spv_t_ptr(&a->s, SpvStorageClassFunction, ty), v, SpvStorageClassFunction);
      s = a2s_slot(a, phi_key(in), 1);
      s->kind = VK_ID, s->id = v;
    }
  }
}

// the incoming values of to's phis for the edge from 'from', stored before the branch
static void edge_stores(struct a2s *a, int from, int to) {
  for (LLVMValueRef in = LLVMGetFirstInstruction(a->b[to].bb); in && LLVMIsAPHINode(in); in = LLVMGetNextInstruction(in)) {
    unsigned n = LLVMCountIncoming(in);

    for (unsigned k = 0; k < n; k++) {
      LLVMValueRef v;
      uint32_t w[2], val;

      if (LLVMGetIncomingBlock(in, k) != a->b[from].bb)
        continue;

      v = LLVMGetIncomingValue(in, k);
      if (LLVMGetTypeKind(LLVMTypeOf(in)) == LLVMPointerTypeKind)
        val = a2s_ptr_bytes(a, v);
      else
        val = a2s_value(a, v);

      w[0] = a2s_slot(a, phi_key(in), 0)->id, w[1] = val;
      a2s_stmt(a, SpvOpStore, w, 2);
      break;
    }
  }
}

// a pointer phi: every incoming pointer must share the root, the offset is what the variable holds
static int phi_root(struct a2s *a, LLVMValueRef phi) {
  unsigned n = LLVMCountIncoming(phi);
  int root = -1;

  for (unsigned k = 0; k < n; k++) {
    LLVMValueRef v = LLVMGetIncomingValue(phi, k);
    struct vslot *s = a2s_slot(a, v, 0);
    int r;

    // a value from a later block or a back edge: its own root is decided when its phi is
    // null (an optional pointer) takes the other incoming values' memory
    if (v == phi || LLVMIsAConstantPointerNull(v))
      continue;
    if (!s && !LLVMIsAConstantExpr(v) && !LLVMIsAGlobalValue(v) && LLVMIsAInstruction(v))
      continue;
    r = a2s_ptr(a, v).root;
    if (root >= 0 && r != root)
      a2s_fail(a, "pointer phi over different memory");
    root = r;
  }

  if (root < 0)
    a2s_fail(a, "pointer phi with no known incoming root");

  return root;
}

static void block_body(struct a2s *a, int i, uint32_t label) {
  LLVMValueRef in, term = LLVMGetBasicBlockTerminator(a->b[i].bb);

  spv_op(&a->body, SpvOpLabel, 1, label);
  a->b[i].emitted = 1;

  for (in = LLVMGetFirstInstruction(a->b[i].bb); in && LLVMIsAPHINode(in); in = LLVMGetNextInstruction(in)) {
    LLVMTypeRef t = LLVMTypeOf(in);
    uint32_t var = a2s_slot(a, phi_key(in), 0)->id;

    if (LLVMGetTypeKind(t) == LLVMPointerTypeKind) {
      struct aptr p = { phi_root(a, in), 0, 0 };
      struct vslot *s;
      p.dyn = a2s_op1(a, SpvOpLoad, spv_t_uint(&a->s), var);
      s = a2s_slot(a, in, 1);
      s->kind = VK_PTR, s->u.p = p;
    } else {
      a2s_set(a, in, a2s_op1(a, SpvOpLoad, a2s_type(a, t), var));
    }
  }

  for (; in && in != term; in = LLVMGetNextInstruction(in)) {
    a2s_inst(a, in);
  }
}

struct ctx {
  // innermost loop: header, exit block, their labels, region end and its label
  int lh, lx, stop;
  uint32_t lxlab, lclab, stoplab;
};

static uint32_t special(struct ctx *c, int t) {
  if (c->stop >= 0 && t == c->stop)
    return c->stoplab;
  if (c->lh >= 0 && t == c->lx)
    return c->lxlab;
  if (c->lh >= 0 && t == c->lh)
    return c->lclab;
  return 0;
}

static int loop_target(struct ctx *c, int t) {
  return c->lh >= 0 && (t == c->lx || t == c->lh) && t != c->stop;
}

static void chain(struct a2s *a, int i, struct ctx *c, uint32_t lab);
static uint32_t enter(struct a2s *a, int t, int from);

// an arm's first block: one with other predecessors is reached through a stub, so the copy decision
// waits until the arm is emitted (the other path may have emitted it by then)
static uint32_t arm_entry(struct a2s *a, int t, int from, uint32_t *stub) {
  *stub = 0;
  if (a->b[t].npred > 1 && !a->b[t].header)
    return *stub = spv_id(&a->s);
  return enter(a, t, from);
}

static void arm(struct a2s *a, int t, int from, struct ctx *c, uint32_t stub) {
  if (stub) {
    spv_op(&a->body, SpvOpLabel, 1, stub);
    spv_op(&a->body, SpvOpBranch, 1, enter(a, t, from));
  }
  chain(a, t, c, 0);
}

// blocks of a small region are emitted again when a second path enters them (short circuits like
// a || b reach the body from two places): the region the block dominates gets fresh labels
#define A2S_DUP_BUDGET 20000

static unsigned block_size(struct a2s *a, int i) {
  unsigned n = 0;

  for (LLVMValueRef in = LLVMGetFirstInstruction(a->b[i].bb); in; in = LLVMGetNextInstruction(in)) {
    n++;
  }
  return n;
}

static uint32_t enter(struct a2s *a, int t, int from) {
  unsigned size = 0;
  int k;

  if (!a->b[t].emitted)
    return a->b[t].label;

  // a back edge to a loop other than the innermost one (a labelled continue) cannot be copied
  if (a->b[t].header || dominates(a, t, from))
    a2s_fail(a, "block %d (%s, from %d) reached twice: control flow not structured", t,
             a->b[t].header ? "loop header" : "back edge", from);

  for (k = 0; k < a->nb; k++) {
    if (a->b[k].reach && dominates(a, t, k))
      size += block_size(a, k);
  }

  if (a->dup_used + size > (getenv("A2S_DUP") ? (unsigned)atoi(getenv("A2S_DUP")) : A2S_DUP_BUDGET))
    a2s_fail(a, "block %d reached twice, copying %u more instructions is over budget: control flow not structured",
             t, size);

  a->dup_used += size;
  for (k = 0; k < a->nb; k++) {
    if (!a->b[k].reach || !dominates(a, t, k))
      continue;
    a->b[k].emitted = 0, a->b[k].merged = 0;
    a->b[k].label = spv_id(&a->s);
  }

  return a->b[t].label;
}

// the label a construct merges at: the block's own, or a stub when the block is taken
static uint32_t merge_label(struct a2s *a, struct ctx *c, int m, int *stub) {
  if (m < 0 || special(c, m) || a->b[m].emitted || a->b[m].merged) {
    *stub = 1;
    return spv_id(&a->s);
  }

  a->b[m].merged = 1;
  *stub = 0;
  return a->b[m].label;
}

static void stub_end(struct a2s *a, struct ctx *c, int m, uint32_t lab) {
  uint32_t sp;

  spv_op(&a->body, SpvOpLabel, 1, lab);
  if (m < 0) {
    spv_op(&a->body, SpvOpUnreachable, 0);
    return;
  }

  sp = special(c, m);
  spv_op(&a->body, SpvOpBranch, 1, sp ? sp : enter(a, m, a->cur));
}

static int loop(struct a2s *a, int h, struct ctx *c) {
  int x = a->b[h].exit, stub;
  uint32_t xlab = merge_label(a, c, x, &stub), cont = spv_id(&a->s), hb = spv_id(&a->s);
  struct ctx in = { h, x, -1, xlab, cont, 0 };

  spv_op(&a->body, SpvOpLabel, 1, a->b[h].label);
  spv_op(&a->body, SpvOpLoopMerge, 3, xlab, cont, 0);
  spv_op(&a->body, SpvOpBranch, 1, hb);
  chain(a, h, &in, hb);
  spv_op(&a->body, SpvOpLabel, 1, cont);
  spv_op(&a->body, SpvOpBranch, 1, a->b[h].label);

  if (stub) {
    stub_end(a, c, x, xlab);
    return -1;
  }

  return x;
}

static int merge_of(struct a2s *a, int i, struct ctx *c) {
  int m = a->b[i].ipdom;

  if (m < 0)
    return -1;

  // a merge outside the loop is never reached through the construct: every arm breaks or continues
  if (c->lh >= 0 && !in_body(a, c->lh, m) && m != c->lx)
    return -1;

  return m;
}

static void chain(struct a2s *a, int i, struct ctx *c, uint32_t lab) {
  for (;;) {
    LLVMValueRef term;
    LLVMOpcode op;

    if (a->b[i].emitted && !(lab && c->lh == i))
    {
      if (getenv("A2S_DEBUG")) {
        char *s1 = LLVMPrintValueToString(LLVMGetBasicBlockTerminator(a->b[i].bb));
        char *s2 = LLVMPrintValueToString(LLVMGetBasicBlockTerminator(a->b[a->cur].bb));
        fprintf(stderr, "reached twice: %s\n  from: %s\n", s1, s2);
      }
      a2s_fail(a, "block %d (from %d) reached twice: control flow not structured", i, a->cur);
    }

    if (a->b[i].header && !(c->lh == i && lab)) {
      i = loop(a, i, c);
      if (i < 0)
        return;
      lab = 0;
      continue;
    }

    block_body(a, i, lab ? lab : a->b[i].label);
    lab = 0;
    a->cur = i;
    term = LLVMGetBasicBlockTerminator(a->b[i].bb);
    op = LLVMGetInstructionOpcode(term);

    if (op == LLVMRet) {
      a2s_return(a, LLVMGetNumOperands(term) ? LLVMGetOperand(term, 0) : 0);
      return;
    }

    if (op == LLVMUnreachable) {
      spv_op(&a->body, SpvOpUnreachable, 0);
      return;
    }

    if (op == LLVMSwitch) {
      int m = merge_of(a, i, c), stub;
      unsigned ns = (unsigned)a->b[i].nsucc, k, n = 0;
      uint32_t mlab, w[1100], sp;
      struct ctx actx;

      // every case reaches the merge without passing through another case's blocks
      for (k = 0; k < ns; k++) {
        edge_stores(a, i, a->b[i].succ[k]);
      }
      uint32_t stubs[512];

      if (ns > 512)
        a2s_fail(a, "switch with too many cases");
      mlab = merge_label(a, c, m, &stub);
      spv_op(&a->body, SpvOpSelectionMerge, 2, mlab, 0);
      w[n++] = a2s_value(a, LLVMGetOperand(term, 0));
      for (k = 0; k < ns && n + 2 < A2S_ARRAY_LEN(w); k++) {
        int s = a->b[i].succ[k];
        uint32_t lab;

        stubs[k] = 0;
        for (unsigned q = 0; q < k; q++) {
          if (a->b[i].succ[q] == s)
            stubs[k] = stubs[q] ? stubs[q] : 0xffffffffu;
        }
        if (s == m)
          lab = mlab;
        else if ((sp = special(c, s)))
          lab = sp;
        else if (stubs[k] == 0xffffffffu)
          lab = a->b[s].label, stubs[k] = 0;
        else if (stubs[k])
          lab = stubs[k];
        else
          lab = arm_entry(a, s, i, &stubs[k]);
        if (k) {
          LLVMValueRef cv = LLVMGetOperand(term, 2 * k);
          w[n++] = (uint32_t)LLVMConstIntGetZExtValue(cv);
        }
        w[n++] = lab;
      }
      if (k < ns)
        a2s_fail(a, "switch with too many cases");
      spv_emit(&a->body, SpvOpSwitch, w, n);

      actx = *c;
      actx.stop = m, actx.stoplab = mlab;
      for (k = 0; k < ns; k++) {
        int s = a->b[i].succ[k], seen = 0;
        for (unsigned q = 0; q < k; q++) {
          seen |= a->b[i].succ[q] == s;
        }
        if (s != m && !special(c, s) && !seen)
          arm(a, s, i, &actx, stubs[k]);
      }

      if (stub) {
        stub_end(a, c, m, mlab);
        return;
      }
      i = m;
      continue;
    }

    if (op != LLVMBr)
      a2s_fail(a, "terminator opcode %d", (int)op);

    if (!LLVMIsConditional(term)) {
      int t = a->b[i].succ[0];
      uint32_t sp = special(c, t);

      edge_stores(a, i, t);
      if (sp) {
        spv_op(&a->body, SpvOpBranch, 1, sp);
        return;
      }
      spv_op(&a->body, SpvOpBranch, 1, enter(a, t, i));
      i = t;
      continue;
    }

    {
      int t = a->b[i].succ[0], f = a->b[i].succ[1], m, stub;
      uint32_t cond = a2s_value(a, LLVMGetCondition(term)), st = special(c, t), sf = special(c, f), mlab;
      struct ctx actx;

      edge_stores(a, i, t);
      if (f != t)
        edge_stores(a, i, f);

      if (st && sf) {
        spv_op(&a->body, SpvOpBranchConditional, 3, cond, st, sf);
        return;
      }

      // a conditional break or continue needs no construct of its own
      if ((loop_target(c, t) && !sf) || (loop_target(c, f) && !st)) {
        int n = loop_target(c, t) ? f : t;
        spv_op(&a->body, SpvOpBranchConditional, 3, cond, st ? st : enter(a, t, i), sf ? sf : enter(a, f, i));
        i = n;
        continue;
      }

      m = merge_of(a, i, c);
      mlab = merge_label(a, c, m, &stub);
      spv_op(&a->body, SpvOpSelectionMerge, 2, mlab, 0);
      {
        uint32_t ts = 0, fs = 0;
        uint32_t tl = t == m ? mlab : st ? st : arm_entry(a, t, i, &ts);
        uint32_t fl = f == m ? mlab : sf ? sf : f == t ? tl : arm_entry(a, f, i, &fs);

        spv_op(&a->body, SpvOpBranchConditional, 3, cond, tl, fl);
        actx = *c;
        actx.stop = m, actx.stoplab = mlab;
        if (t != m && !st)
          arm(a, t, i, &actx, ts);
        if (f != m && !sf && f != t)
          arm(a, f, i, &actx, fs);
      }

      if (stub) {
        stub_end(a, c, m, mlab);
        return;
      }

      i = m;
    }
  }
}

void a2s_cfg(struct a2s *a) {
  struct ctx c = { -1, -1, -1, 0, 0, 0 };

  build(a);
  phi_vars(a);
  a->first_label = a->b[0].label;
  chain(a, 0, &c, 0);
}
