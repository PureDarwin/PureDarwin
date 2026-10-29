// airify IN.ll [-o OUT.bc] [-S OUT.ll] [-m OUT.metallib -e ENTRY]: clang's IR for an MSL file to AIR: entry
// functions and air.* metadata, everything inlined, address spaces resolved, intrinsics as air.* calls
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ReplaceConstant.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"

#include <map>
#include <string>
#include <vector>

using namespace llvm;

static const char *const air_layout =
    "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-"
    "v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024-n8:16:32";
static const char *const air_triple = "air64_v27-apple-macosx15.0.0";

enum { AS_PRIVATE = 0, AS_DEVICE = 1, AS_CONSTANT = 2, AS_THREADGROUP = 3, AS_GENERIC = 4 };

[[noreturn]] static void fail(const std::string &m) {
  errs() << "airify: " << m << "\n";
  exit(1);
}

static std::vector<std::string> split(const std::string &s, char c) {
  std::vector<std::string> r;
  size_t p = 0, q;

  while ((q = s.find(c, p)) != std::string::npos) {
    r.push_back(s.substr(p, q - p));
    p = q + 1;
  }
  r.push_back(s.substr(p));
  return r;
}

// one descriptor line split at '|', with the F/S lines that follow a parameter
struct Line {
  std::vector<std::string> f;
  std::vector<Line> sub;
};

struct Entry {
  Function *fn;
  std::string stage, name;
  Line ret;
  std::vector<Line> outs, params;
};

static Type *lty(LLVMContext &c, const std::string &s) {
  size_t x = s.find('x');
  std::string e = x == std::string::npos ? s : s.substr(0, x);
  unsigned n = x == std::string::npos ? 0 : (unsigned)atoi(s.c_str() + x + 1);
  Type *t;

  if (e == "f32")
    t = Type::getFloatTy(c);
  else if (e == "f16")
    t = Type::getHalfTy(c);
  else if (e == "b1")
    t = Type::getInt1Ty(c);
  else if (e.size() > 1 && (e[0] == 's' || e[0] == 'u'))
    t = Type::getIntNTy(c, (unsigned)atoi(e.c_str() + 1));
  else
    fail("value type " + s);
  return n ? (Type *)FixedVectorType::get(t, n) : t;
}

static std::string attr_name(const std::string &a) {
  size_t p = a.find('(');
  return p == std::string::npos ? a : a.substr(0, p);
}

static unsigned attr_int(const std::string &a) {
  size_t p = a.find('(');
  return p == std::string::npos ? 0 : (unsigned)atoi(a.c_str() + p + 1);
}

static std::string attr_arg(const std::string &a) {
  size_t p = a.find('('), q = a.rfind(')');
  return p == std::string::npos ? "" : a.substr(p + 1, q - p - 1);
}

static bool has(const std::vector<std::string> &attrs, const std::string &n, std::string *out = nullptr) {
  for (auto &a : attrs) {
    if (attr_name(a) != n)
      continue;
    if (out)
      *out = a;
    return true;
  }
  return false;
}

struct Md {
  LLVMContext &c;
  std::vector<Metadata *> v;

  Md(LLVMContext &ctx) : c(ctx) {}
  Md &s(const std::string &x) {
    v.push_back(MDString::get(c, x));
    return *this;
  }
  Md &i(unsigned x) {
    v.push_back(ConstantAsMetadata::get(ConstantInt::get(Type::getInt32Ty(c), x)));
    return *this;
  }
  Md &n(Metadata *m) {
    v.push_back(m);
    return *this;
  }
  MDNode *get() {
    return MDNode::get(c, v);
  }
};

// a [[function_constant(n)]] variable: "FC|index|type|name" from the plugin
struct FuncConst {
  std::string global, type, name;
  unsigned index;
};
static std::vector<FuncConst> fconsts;

static std::vector<Entry> read_entries(Module &m) {
  std::vector<Entry> es;
  GlobalVariable *ga = m.getGlobalVariable("llvm.global.annotations");

  if (!ga)
    return es;

  auto *arr = cast<ConstantArray>(ga->getInitializer());
  for (unsigned i = 0; i < arr->getNumOperands(); i++) {
    auto *cs = cast<ConstantStruct>(arr->getOperand(i));
    auto *fn = dyn_cast<Function>(cs->getOperand(0)->stripPointerCasts());
    auto *gv = dyn_cast<GlobalVariable>(cs->getOperand(0)->stripPointerCasts());
    auto *sg = dyn_cast<GlobalVariable>(cs->getOperand(1)->stripPointerCasts());
    if ((!fn && !gv) || !sg || !sg->hasInitializer())
      continue;
    auto *cda = dyn_cast<ConstantDataArray>(sg->getInitializer());
    if (!cda || !cda->isCString())
      continue;
    std::string d = cda->getAsCString().str();
    if (gv && !d.compare(0, 3, "FC|")) {
      std::vector<std::string> f = split(d, '|');
      // loads must not see the zero initializer: the value comes when the pipeline is specialized
      gv->setExternallyInitialized(true);
      fconsts.push_back({ gv->getName().str(), f[2], f[3], (unsigned)atoi(f[1].c_str()) });
      continue;
    }
    if (!fn || d.compare(0, 2, "E|"))
      continue;

    Entry e;
    e.fn = fn;
    for (auto &l : split(d, '\n')) {
      if (l.empty())
        continue;
      Line ln;
      ln.f = split(l, '|');
      char k = ln.f[0][0];
      if (k == 'E')
        e.stage = ln.f[1], e.name = ln.f[2];
      else if (k == 'R')
        e.ret = ln;
      else if (k == 'O')
        e.outs.push_back(ln);
      else if (k == 'P')
        e.params.push_back(ln);
      else if ((k == 'F' || k == 'S' || k == 'T') && !e.params.empty())
        e.params.back().sub.push_back(ln);
    }
    es.push_back(e);
  }

  ga->eraseFromParent();
  return es;
}

static Value *byte_ptr(IRBuilder<> &b, Value *base, unsigned off) {
  return b.CreateConstInBoundsGEP1_32(b.getInt8Ty(), base, off);
}

// fragment interpolation from the field's attributes: AIR's sampling and perspective words
static void interp(Md &md, const std::vector<std::string> &a) {
  if (has(a, "flat")) {
    md.s("air.flat");
    return;
  }
  const char *where = "air.center", *persp = "air.perspective";
  for (auto &x : a) {
    std::string n = attr_name(x);
    if (n == "center_no_perspective")
      persp = "air.no_perspective";
    else if (n == "centroid_perspective")
      where = "air.centroid";
    else if (n == "centroid_no_perspective")
      where = "air.centroid", persp = "air.no_perspective";
    else if (n == "sample_perspective")
      where = "air.sample";
    else if (n == "sample_no_perspective")
      where = "air.sample", persp = "air.no_perspective";
  }
  md.s(where).s(persp);
}

// the name Apple gives a varying without an attribute: generated(<length><name><Itanium type>), as in
// "generated(10v_texCoordDv3_f)" for a float3 v_texCoord
static std::string generated_name(const std::string &lt, const std::string &tname, const std::string &name) {
  size_t x = lt.find('x');
  std::string e = x == std::string::npos ? lt : lt.substr(0, x), m;
  bool u = !tname.compare(0, 1, "u");

  if (e == "f32")
    m = "f";
  else if (e == "f16")
    m = "Dh";
  else if (e == "i32")
    m = u ? "j" : "i";
  else if (e == "i16")
    m = u ? "t" : "s";
  else if (e == "i8")
    m = u ? "h" : "c";
  else
    m = "v";
  if (x != std::string::npos)
    m = "Dv" + lt.substr(x + 1) + "_" + m;
  return "generated(" + std::to_string(name.size()) + name + m + ")";
}

// an output (return value or member) and its AIR description
static MDNode *output_md(LLVMContext &c, const std::string &stage, const std::vector<std::string> &a,
                         const std::string &tname, const std::string &name, const std::string &lt = "") {
  Md md(c);
  std::string x;

  if (has(a, "user", &x) && stage == "vertex")
    md.s("air.vertex_output").s(x);
  else if (stage == "vertex" && (a.empty() || (a.size() == 1 && a[0].empty())) && !name.empty())
    md.s("air.vertex_output").s(generated_name(lt, tname, name));
  else if (has(a, "position"))
    md.s("air.position");
  else if (has(a, "point_size"))
    md.s("air.point_size");
  else if (has(a, "render_target_array_index"))
    md.s("air.render_target_array_index");
  else if (has(a, "viewport_array_index"))
    md.s("air.viewport_array_index");
  else if (has(a, "clip_distance"))
    md.s("air.clip_distance");
  else if (has(a, "color", &x)) {
    auto args = split(attr_arg(x), ',');
    md.s("air.render_target").i((unsigned)atoi(args[0].c_str())).i(args.size() > 1 ? (unsigned)atoi(args[1].c_str()) : 0);
  } else if (has(a, "depth", &x))
    md.s("air.depth").s("air.depth_qualifier").s("air." + attr_arg(x));
  else if (has(a, "sample_mask"))
    md.s("air.sample_mask");
  else if (stage == "fragment")
    md.s("air.render_target").i(0).i(0);
  md.s("air.arg_type_name").s(tname);
  if (!name.empty())
    md.s("air.arg_name").s(name);
  return md.get();
}

// a builtin value's AIR kind: the attribute name with air. in front, fragment positions interpolated
static void builtin_md(Md &md, const std::string &stage, const std::vector<std::string> &a) {
  std::string n = a.empty() ? "?" : attr_name(a[0]);

  md.s("air." + n);
  if (n == "position" && stage == "fragment")
    md.s("air.center").s("air.no_perspective");
}

// one level of struct info lines (S, or T for a member's own struct): offset, element size, count, type, name,
// and for an argument buffer member its own argument description
static MDNode *struct_info(LLVMContext &c, const std::vector<Line> &ls) {
  Md si(c);

  for (size_t k = 0; k < ls.size(); k++) {
    auto &sl = ls[k];
    if (sl.f[0] != "S")
      continue;
    si.i((unsigned)atoi(sl.f[1].c_str())).i((unsigned)atoi(sl.f[2].c_str())).i((unsigned)atoi(sl.f[3].c_str()));
    si.s(sl.f[4]).s(sl.f[5]);
    if (sl.f.size() < 7 || sl.f[6].empty())
      continue;

    // kind;id;address space;access;size;align;type name
    auto d = split(sl.f[6], ';');
    unsigned id = (unsigned)atoi(d[1].c_str());
    Md am(c);
    am.i(id);
    if (d[0] == "buffer") {
      std::vector<Line> inner;
      for (size_t j = k + 1; j < ls.size() && ls[j].f[0] == "T"; j++) {
        Line t = ls[j];
        t.f[0] = "S";
        inner.push_back(t);
      }
      am.s("air.buffer").s("air.location_index").i(id).i(1).s("air." + d[3]).s("air.address_space").i(
          (unsigned)atoi(d[2].c_str()));
      if (!inner.empty())
        am.s("air.struct_type_info").n(struct_info(c, inner));
      if (!d[4].empty())
        am.s("air.arg_type_size").i((unsigned)atoi(d[4].c_str())).s("air.arg_type_align_size").i(
            (unsigned)atoi(d[5].c_str()));
    } else if (d[0] == "texture") {
      am.s("air.texture").s("air.location_index").i(id).i(1).s("air." + d[3]);
    } else {
      am.s("air.sampler").s("air.location_index").i(id).i(1);
    }
    am.s("air.arg_type_name").s(d[6]).s("air.arg_name").s(sl.f[5]);
    si.s("air.indirect_argument").n(am.get());
  }
  return si.get();
}

static Function *build_entry(Module &m, Entry &e) {
  LLVMContext &c = m.getContext();
  Function *f = e.fn;
  bool sret = f->arg_size() && f->hasParamAttribute(0, Attribute::StructRet);
  std::vector<Type *> pts;
  std::vector<Metadata *> args, outs;
  // per source parameter: how its G arguments start
  std::vector<unsigned> first;
  Type *rt;

  // AIR parameters: stage_in structs open into one parameter per member
  for (auto &p : e.params) {
    const std::string &k = p.f[2];
    first.push_back((unsigned)pts.size());
    if (k == "buffer")
      pts.push_back(PointerType::get(c, (unsigned)atoi(p.f[4].c_str())));
    else if (k == "texture")
      pts.push_back(PointerType::get(c, AS_DEVICE));
    else if (k == "sampler")
      pts.push_back(PointerType::get(c, AS_CONSTANT));
    else if (k == "builtin")
      pts.push_back(lty(c, p.f[4]));
    else if (k == "stage_in") {
      for (auto &fl : p.sub) {
        pts.push_back(lty(c, fl.f[2]));
      }
    } else {
      fail("parameter kind " + k);
    }
  }

  if (e.ret.f[1] == "void") {
    rt = Type::getVoidTy(c);
  } else if (e.ret.f[1] == "struct") {
    std::vector<Type *> ms;
    for (auto &o : e.outs) {
      ms.push_back(lty(c, o.f[2]));
    }
    rt = StructType::get(c, ms, true);
  } else {
    rt = lty(c, e.ret.f[2]);
  }

  std::string uname = f->getName().str();
  f->setName(uname + ".msl");
  Function *g = Function::Create(FunctionType::get(rt, pts, false), GlobalValue::ExternalLinkage, e.name, m);
  g->addFnAttr(Attribute::NoUnwind);
  BasicBlock *bb = BasicBlock::Create(c, "", g);
  IRBuilder<> b(bb);
  std::vector<Value *> cargs;
  Value *ret_slot = nullptr;

  if (sret) {
    Type *st = f->getParamStructRetType(0);
    ret_slot = b.CreateAlloca(st);
    cargs.push_back(ret_slot);
  }

  for (unsigned i = 0; i < e.params.size(); i++) {
    auto &p = e.params[i];
    const std::string &k = p.f[2];
    unsigned fi = i + (sret ? 1 : 0), gi = first[i];
    Type *ft = f->getArg(fi)->getType();
    Value *v = g->getArg(gi);
    Md md(c);

    if (k == "stage_in") {
      // the struct the user function takes, filled from the member parameters
      Type *st = f->getParamByValType(fi);
      if (!st)
        fail("stage_in parameter not passed by value");
      Value *slot = b.CreateAlloca(st);
      for (unsigned j = 0; j < p.sub.size(); j++) {
        auto &fl = p.sub[j];
        auto a = split(fl.f[4], ',');
        std::string x;
        Value *gv = g->getArg(gi + j);
        b.CreateStore(gv, byte_ptr(b, slot, (unsigned)atoi(fl.f[1].c_str())));
        Md fm(c);
        fm.i(gi + j);
        if (e.stage == "vertex" && has(a, "attribute", &x))
          fm.s("air.vertex_input").s("air.location_index").i(attr_int(x)).i(1);
        else if (has(a, "user", &x))
          fm.s("air.fragment_input").s(x), interp(fm, a);
        else if (e.stage == "fragment" && (a.empty() || (a.size() == 1 && a[0].empty())))
          fm.s("air.fragment_input").s(generated_name(fl.f[2], fl.f[3], fl.f[5])), interp(fm, a);
        else
          builtin_md(fm, e.stage, a);
        fm.s("air.arg_type_name").s(fl.f[3]).s("air.arg_name").s(fl.f[5]);
        args.push_back(fm.get());
      }
      cargs.push_back(slot);
      continue;
    }

    md.i(gi);
    if (k == "buffer") {
      unsigned as = (unsigned)atoi(p.f[4].c_str());
      bool indirect = false;
      for (auto &sl : p.sub) {
        indirect |= sl.f[0] == "S" && sl.f.size() > 6 && !sl.f[6].empty();
      }
      md.s(indirect ? "air.indirect_buffer" : "air.buffer");
      if (p.f[6] == "ref")
        md.s("air.buffer_size").i((unsigned)atoi(p.f[7].c_str()));
      md.s("air.location_index").i((unsigned)atoi(p.f[3].c_str())).i(1).s("air." + p.f[5]);
      md.s("air.address_space").i(as);
      if (!p.sub.empty())
        md.s("air.struct_type_info").n(struct_info(c, p.sub));
      md.s("air.arg_type_size").i((unsigned)atoi(p.f[7].c_str()));
      md.s("air.arg_type_align_size").i((unsigned)atoi(p.f[8].c_str()));
      md.s("air.arg_type_name").s(p.f[9]).s("air.arg_name").s(p.f[10]);
      if (ft->getPointerAddressSpace() != as)
        v = b.CreateAddrSpaceCast(v, ft);
    } else if (k == "texture" || k == "sampler") {
      // the handle class the user function takes by value
      Type *st = f->getParamByValType(fi);
      if (!st)
        fail(k + " parameter not passed by value");
      Value *slot = b.CreateAlloca(st);
      b.CreateStore(v, slot);
      v = slot;
      if (k == "texture")
        md.s("air.texture").s("air.location_index").i((unsigned)atoi(p.f[3].c_str())).i(1).s("air." + p.f[5]).s(
            "air.arg_type_name").s(p.f[4]).s("air.arg_name").s(p.f[6]);
      else
        md.s("air.sampler").s("air.location_index").i((unsigned)atoi(p.f[3].c_str())).i(1).s("air.arg_type_name").s(
            "sampler").s("air.arg_name").s(p.f[4]);
    } else {
      builtin_md(md, e.stage, split(p.f[3], ','));
      md.s("air.arg_type_name").s(p.f[5]).s("air.arg_name").s(p.f[6]);
    }
    args.push_back(md.get());
    cargs.push_back(v);
  }

  CallInst *call = b.CreateCall(f, cargs);
  call->setCallingConv(CallingConv::C);
  if (e.ret.f[1] == "void") {
    b.CreateRetVoid();
  } else if (e.ret.f[1] == "struct") {
    Value *r = PoisonValue::get(rt);
    for (unsigned j = 0; j < e.outs.size(); j++) {
      auto &o = e.outs[j];
      Value *x = b.CreateLoad(lty(c, o.f[2]), byte_ptr(b, ret_slot, (unsigned)atoi(o.f[1].c_str())));
      r = b.CreateInsertValue(r, x, j);
      outs.push_back(output_md(c, e.stage, split(o.f[4], ','), o.f[3], o.f[5], o.f[2]));
    }
    b.CreateRet(r);
  } else {
    b.CreateRet(call);
    outs.push_back(output_md(c, e.stage, split(e.ret.f[4], ','), e.ret.f[3], ""));
  }

  NamedMDNode *list = m.getOrInsertNamedMetadata("air." + e.stage);
  list->addOperand(MDNode::get(c, { ValueAsMetadata::get(g), MDNode::get(c, outs), MDNode::get(c, args) }));
  return g;
}

// the same attributes without the return value's
static AttributeList no_ret(LLVMContext &c, AttributeList al, unsigned np) {
  std::vector<AttributeSet> ps;

  for (unsigned i = 0; i < np; i++) {
    ps.push_back(al.getParamAttrs(i));
  }
  return AttributeList::get(c, al.getFnAttrs(), AttributeSet(), ps);
}

// calls named pd.sret.air.X return just the texel: AIR's X returns { texel, i8 }
static void fix_sret_calls(Module &m) {
  std::vector<Function *> fs;

  for (Function &f : m) {
    if (f.getName().starts_with("pd.sret."))
      fs.push_back(&f);
  }

  for (Function *f : fs) {
    LLVMContext &c = m.getContext();
    std::string n = f->getName().substr(8).str();
    Type *rt = StructType::get(c, { f->getReturnType(), Type::getInt8Ty(c) });
    FunctionType *ft = FunctionType::get(rt, f->getFunctionType()->params(), false);
    FunctionCallee nf = m.getOrInsertFunction(n, ft, no_ret(c, f->getAttributes(), f->arg_size()));

    for (User *u : make_early_inc_range(f->users())) {
      auto *ci = cast<CallInst>(u);
      IRBuilder<> b(ci);
      std::vector<Value *> a(ci->arg_begin(), ci->arg_end());
      CallInst *nc = b.CreateCall(nf, a);
      nc->setAttributes(no_ret(c, ci->getAttributes(), ci->arg_size()));
      ci->replaceAllUsesWith(b.CreateExtractValue(nc, 0));
      ci->eraseFromParent();
    }
    f->eraseFromParent();
  }
}

// users of old (a pointer) moved to nw, the same memory in another address space, false when one could
// not move (old keeps that use)
static bool move_users(Value *old, Value *nw) {
  bool all = true;
  unsigned nas = nw->getType()->getPointerAddressSpace();

  for (Use &u : make_early_inc_range(old->uses())) {
    auto *in = dyn_cast<Instruction>(u.getUser());
    if (!in) {
      all = false;
      continue;
    }

    if (auto *gep = dyn_cast<GetElementPtrInst>(in)) {
      if (u.getOperandNo() != 0) {
        all = false;
        continue;
      }
      IRBuilder<> b(gep);
      std::vector<Value *> idx(gep->idx_begin(), gep->idx_end());
      Value *ng = b.CreateGEP(gep->getSourceElementType(), nw, idx, gep->getName(), gep->getNoWrapFlags());
      if (move_users(gep, ng))
        gep->eraseFromParent();
      else
        all = false;
    } else if (isa<LoadInst>(in) || isa<PtrToIntInst>(in) || (isa<StoreInst>(in) && u.getOperandNo() == 1)) {
      u.set(nw);
    } else if (auto *ac = dyn_cast<AddrSpaceCastInst>(in)) {
      if (ac->getDestAddressSpace() == nas) {
        ac->replaceAllUsesWith(nw);
        ac->eraseFromParent();
      } else {
        u.set(nw);
      }
    } else if (auto *mi = dyn_cast<MemIntrinsic>(in)) {
      IRBuilder<> b(mi);
      Value *dst = mi->getRawDest() == old ? nw : mi->getRawDest();
      if (auto *mt = dyn_cast<MemTransferInst>(mi)) {
        Value *src = mt->getRawSource() == old ? nw : mt->getRawSource();
        if (isa<MemMoveInst>(mt))
          b.CreateMemMove(dst, mt->getDestAlign(), src, mt->getSourceAlign(), mt->getLength(), mt->isVolatile());
        else
          b.CreateMemCpy(dst, mt->getDestAlign(), src, mt->getSourceAlign(), mt->getLength(), mt->isVolatile());
      } else {
        auto *ms = cast<MemSetInst>(mi);
        b.CreateMemSet(dst, ms->getValue(), ms->getLength(), ms->getDestAlign(), ms->isVolatile());
      }
      mi->eraseFromParent();
    } else if (isa<IntrinsicInst>(in) && cast<IntrinsicInst>(in)->isAssumeLikeIntrinsic()) {
      in->eraseFromParent();
    } else {
      all = false;
    }
  }
  return all;
}

// every address space cast out of a known space folds into its users: the generic space disappears and
// buffers end in AIR's spaces
static unsigned resolve_casts(Module &m, bool loads) {
  unsigned n = 0;
  bool again = true;

  while (again) {
    again = false;
    std::vector<Constant *> cexprs;
    for (GlobalVariable &g : m.globals()) {
      for (User *u : g.users()) {
        if (auto *ce = dyn_cast<ConstantExpr>(u))
          cexprs.push_back(ce);
      }
    }
    if (!cexprs.empty())
      convertUsersOfConstantsToInstructions(cexprs);

    std::vector<AddrSpaceCastInst *> casts;
    for (Function &f : m) {
      for (Instruction &in : instructions(f)) {
        if (auto *ac = dyn_cast<AddrSpaceCastInst>(&in))
          casts.push_back(ac);
      }
    }
    // a generic pointer read from memory (an argument buffer member) can only be MSL's constant: the
    // front end spells constant as __generic. Only once sroa has removed the spilled this pointers
    std::vector<LoadInst *> gloads;
    for (Function &f : m) {
      for (Instruction &in : instructions(f)) {
        auto *ld = dyn_cast<LoadInst>(&in);
        if (loads && ld && ld->getType()->isPointerTy() && ld->getType()->getPointerAddressSpace() == AS_GENERIC)
          gloads.push_back(ld);
      }
    }
    for (LoadInst *ld : gloads) {
      IRBuilder<> b(ld);
      LoadInst *nl = b.CreateAlignedLoad(PointerType::get(m.getContext(), AS_CONSTANT), ld->getPointerOperand(),
                                         ld->getAlign());
      nl->copyMetadata(*ld);
      if (move_users(ld, nl)) {
        ld->eraseFromParent();
        n++, again = true;
      } else {
        nl->eraseFromParent();
      }
    }

    for (AddrSpaceCastInst *ac : casts) {
      if (ac->getSrcAddressSpace() == AS_GENERIC)
        continue;
      if (move_users(ac, ac->getPointerOperand())) {
        ac->eraseFromParent();
        n++, again = true;
      }
    }
  }
  return n;
}

// program scope constants: the plugin put them in __global, AIR keeps them in constant
static void constant_globals(Module &m) {
  std::vector<GlobalVariable *> gs;

  for (GlobalVariable &g : m.globals()) {
    if (g.getAddressSpace() == AS_DEVICE && g.hasInitializer() && !g.getName().starts_with("llvm."))
      gs.push_back(&g);
  }

  for (GlobalVariable *g : gs) {
    // globals a static initialiser writes stay writable, as in Apple's output
    auto *ng = new GlobalVariable(m, g->getValueType(), g->isConstant(), g->getLinkage(), g->getInitializer(), "",
                                  nullptr, GlobalValue::NotThreadLocal, AS_CONSTANT);
    ng->takeName(g);
    ng->setExternallyInitialized(g->isExternallyInitialized());
    ng->setAlignment(g->getAlign());
    g->replaceAllUsesWith(ConstantExpr::getAddrSpaceCast(ng, g->getType()));
    g->eraseFromParent();
  }
}

static std::string type_suffix(Type *t) {
  std::string s;
  Type *e = t->getScalarType();

  if (auto *vt = dyn_cast<FixedVectorType>(t))
    s = "v" + std::to_string(vt->getNumElements());
  if (e->isFloatTy())
    return s + "f32";
  if (e->isHalfTy())
    return s + "f16";
  return s + "i" + std::to_string(e->getIntegerBitWidth());
}

// LLVM intrinsics the optimizer introduced, as the air.* calls the translator reads
static void lower_intrinsics(Module &m) {
  struct {
    Intrinsic::ID id;
    const char *air;
    unsigned nargs;
  } map[] = {
    { Intrinsic::smax, "max.s", 2 },       { Intrinsic::umax, "max.u", 2 },     { Intrinsic::smin, "min.s", 2 },
    { Intrinsic::umin, "min.u", 2 },       { Intrinsic::abs, "abs.s", 1 },      { Intrinsic::fabs, "fabs", 1 },
    { Intrinsic::minnum, "fmin", 2 },      { Intrinsic::maxnum, "fmax", 2 },    { Intrinsic::floor, "floor", 1 },
    { Intrinsic::ceil, "ceil", 1 },        { Intrinsic::trunc, "trunc", 1 },    { Intrinsic::rint, "rint", 1 },
    { Intrinsic::round, "round", 1 },      { Intrinsic::sqrt, "sqrt", 1 },      { Intrinsic::ctpop, "popcount", 1 },
    { Intrinsic::ctlz, "clz", 1 },         { Intrinsic::cttz, "ctz", 1 },       { Intrinsic::bitreverse, "reverse_bits", 1 },
    { Intrinsic::exp2, "exp2", 1 },        { Intrinsic::log2, "log2", 1 },      { Intrinsic::sin, "sin", 1 },
    { Intrinsic::cos, "cos", 1 },          { Intrinsic::pow, "pow", 2 },
  };
  std::vector<IntrinsicInst *> work;

  for (Function &f : m) {
    for (Instruction &in : instructions(f)) {
      if (auto *ii = dyn_cast<IntrinsicInst>(&in))
        work.push_back(ii);
    }
  }

  for (IntrinsicInst *ii : work) {
    Intrinsic::ID id = ii->getIntrinsicID();
    Function *pf = ii->getFunction();

    if (id == Intrinsic::fmuladd || id == Intrinsic::fma)
      continue;
    if (ii->isAssumeLikeIntrinsic() || id == Intrinsic::experimental_noalias_scope_decl) {
      ii->eraseFromParent();
      continue;
    }
    if (auto *mc = dyn_cast<MemCpyInst>(ii)) {
      TargetTransformInfo tti(m.getDataLayout());
      expandMemCpyAsLoop(mc, tti);
      mc->eraseFromParent();
      continue;
    }
    if (auto *ms = dyn_cast<MemSetInst>(ii)) {
      expandMemSetAsLoop(ms);
      ms->eraseFromParent();
      continue;
    }

    bool done = false;
    for (auto &e : map) {
      if (e.id != id)
        continue;
      Type *t = ii->getType();
      std::vector<Type *> ps(e.nargs, t);
      std::string n = std::string("air.") + e.air + "." + type_suffix(t);
      FunctionCallee fc = m.getOrInsertFunction(n, FunctionType::get(t, ps, false));
      IRBuilder<> b(ii);
      std::vector<Value *> a;
      for (unsigned k = 0; k < e.nargs; k++) {
        a.push_back(ii->getArgOperand(k));
      }
      CallInst *nc = b.CreateCall(fc, a);
      nc->setFastMathFlags(isa<FPMathOperator>(ii) ? ii->getFastMathFlags() : FastMathFlags());
      ii->replaceAllUsesWith(nc);
      ii->eraseFromParent();
      done = true;
      break;
    }
    if (!done)
      errs() << "airify: " << pf->getName() << ": intrinsic " << ii->getCalledFunction()->getName() << " left as is\n";
  }
}

// float conversions as AIR spells them: air.convert.<to kind>.<to type>.<from kind>.<from type>
static void lower_converts(Module &m) {
  std::vector<CastInst *> work;

  for (Function &f : m) {
    for (Instruction &in : instructions(f)) {
      auto *ci = dyn_cast<CastInst>(&in);
      if (!ci)
        continue;
      switch (ci->getOpcode()) {
      case Instruction::FPExt:
      case Instruction::FPTrunc:
      case Instruction::UIToFP:
      case Instruction::SIToFP:
      case Instruction::FPToUI:
      case Instruction::FPToSI:
        work.push_back(ci);
        break;
      default:
        break;
      }
    }
  }

  for (CastInst *ci : work) {
    unsigned op = ci->getOpcode();
    char to = op == Instruction::FPToUI ? 'u' : op == Instruction::FPToSI ? 's' : 'f';
    char from = op == Instruction::UIToFP ? 'u' : op == Instruction::SIToFP ? 's' : 'f';
    Type *dt = ci->getType(), *st = ci->getOperand(0)->getType();
    std::string n = std::string("air.convert.") + to + "." + type_suffix(dt) + "." + from + "." + type_suffix(st);
    FunctionCallee fc = m.getOrInsertFunction(n, FunctionType::get(dt, { st }, false));
    IRBuilder<> b(ci);
    CallInst *nc = b.CreateCall(fc, { ci->getOperand(0) });
    if (auto *fn = dyn_cast<Function>(fc.getCallee()))
      fn->setDoesNotAccessMemory(), fn->setDoesNotThrow();
    ci->replaceAllUsesWith(nc);
    ci->eraseFromParent();
  }
}

static void run(Module &m, const std::string &pipeline) {
  PassBuilder pb;
  LoopAnalysisManager lam;
  FunctionAnalysisManager fam;
  CGSCCAnalysisManager cam;
  ModuleAnalysisManager mam;

  pb.registerModuleAnalyses(mam);
  pb.registerCGSCCAnalyses(cam);
  pb.registerFunctionAnalyses(fam);
  pb.registerLoopAnalyses(lam);
  pb.crossRegisterProxies(lam, fam, cam, mam);

  ModulePassManager mpm;
  if (pipeline == "O2") {
    mpm = pb.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
  } else if (auto err = pb.parsePassPipeline(mpm, pipeline)) {
    fail(toString(std::move(err)));
  }
  mpm.run(m, mam);
}

// pd.const_sampler(state) calls: one internal __air_sampler_state global per state, listed in
// !air.sampler_states, as Apple's compiler emits constexpr samplers
static void const_samplers(Module &m) {
  Function *f = m.getFunction("pd.const_sampler");
  LLVMContext &c = m.getContext();
  std::map<uint64_t, GlobalVariable *> made;

  if (!f)
    return;
  for (User *u : make_early_inc_range(f->users())) {
    auto *call = dyn_cast<CallInst>(u);
    if (!call)
      continue;
    auto *k = dyn_cast<ConstantInt>(call->getArgOperand(0));
    if (!k)
      fail("constexpr sampler whose state is not a constant in " + call->getFunction()->getName().str());
    GlobalVariable *&g = made[k->getZExtValue()];
    if (!g) {
      // Xcode 26's shape: [2 x i64] { state, 0 }
      ArrayType *at = ArrayType::get(Type::getInt64Ty(c), 2);
      Constant *init = ConstantArray::get(at, { k, ConstantInt::get(Type::getInt64Ty(c), 0) });
      g = new GlobalVariable(m, at, true, GlobalValue::InternalLinkage, init, "__air_sampler_state", nullptr,
                             GlobalValue::NotThreadLocal, AS_CONSTANT);
      g->setAlignment(Align(8));
      m.getOrInsertNamedMetadata("air.sampler_states")
          ->addOperand(MDNode::get(c, { MDString::get(c, "air.sampler_state"), ValueAsMetadata::get(g) }));
    }
    call->replaceAllUsesWith(ConstantExpr::getPointerBitCastOrAddrSpaceCast(g, call->getType()));
    call->eraseFromParent();
  }
  if (f->use_empty())
    f->eraseFromParent();
}

// function constants in Apple's shape: an externally initialized constant in air.fc_initializer, named in
// !air.function_constants, its static initialisers in air.static_init, is_function_constant_defined on space 2
static void function_constants(Module &m) {
  LLVMContext &c = m.getContext();

  for (auto &fc : fconsts) {
    GlobalVariable *g = m.getGlobalVariable(fc.global, true);
    if (!g)
      continue;
    g->setInitializer(UndefValue::get(g->getValueType()));
    g->setConstant(true);
    g->setExternallyInitialized(true);
    g->setLinkage(GlobalValue::LinkOnceODRLinkage);
    g->setVisibility(GlobalValue::HiddenVisibility);
    g->setSection("air.fc_initializer");
    m.getOrInsertNamedMetadata("air.function_constants")
        ->addOperand(MDNode::get(c, { ValueAsMetadata::get(g), MDString::get(c, fc.type), MDString::get(c, fc.name),
                                      ConstantAsMetadata::get(ConstantInt::get(Type::getInt32Ty(c), fc.index)) }));
  }
  // loads and stores of constant space globals name them directly, as Apple's do
  for (GlobalVariable &g : m.globals()) {
    if (g.getAddressSpace() != AS_CONSTANT)
      continue;
    for (User *u : make_early_inc_range(g.users())) {
      auto *ce = dyn_cast<ConstantExpr>(u);
      if (!ce || ce->getOpcode() != Instruction::AddrSpaceCast)
        continue;
      for (User *cu : make_early_inc_range(ce->users())) {
        if (auto *ld = dyn_cast<LoadInst>(cu))
          ld->setOperand(0, &g);
        else if (auto *st = dyn_cast<StoreInst>(cu); st && st->getPointerOperand() == ce)
          st->setOperand(1, &g);
      }
    }
  }
  if (GlobalVariable *ctors = m.getGlobalVariable("llvm.global_ctors")) {
    if (auto *arr = dyn_cast<ConstantArray>(ctors->getInitializer())) {
      for (Value *op : arr->operands()) {
        if (auto *fn = dyn_cast<Function>(cast<ConstantStruct>(op)->getOperand(1)->stripPointerCasts()))
        {
          fn->setSection("air.static_init");
          // spir kernel argument metadata means nothing to AIR
          for (const char *k : { "kernel_arg_addr_space", "kernel_arg_access_qual", "kernel_arg_type",
                                 "kernel_arg_base_type", "kernel_arg_type_qual" })
            fn->setMetadata(k, nullptr);
        }
      }
    }
  }
  if (Function *d = m.getFunction("air.is_function_constant_defined")) {
    FunctionType *ft = FunctionType::get(Type::getInt1Ty(c), { PointerType::get(c, AS_CONSTANT) }, false);
    d->setName("");
    Function *nd = Function::Create(ft, GlobalValue::ExternalLinkage, "air.is_function_constant_defined", m);
    for (User *u : make_early_inc_range(d->users())) {
      auto *call = dyn_cast<CallInst>(u);
      if (!call)
        continue;
      IRBuilder<> b(call);
      Value *p = call->getArgOperand(0)->stripPointerCasts();
      if (p->getType()->getPointerAddressSpace() != AS_CONSTANT)
        p = b.CreateAddrSpaceCast(p, PointerType::get(c, AS_CONSTANT));
      Value *r = b.CreateCall(nd, { p });
      if (call->getType() != r->getType())
        r = b.CreateZExt(r, call->getType());
      call->replaceAllUsesWith(r);
      call->eraseFromParent();
    }
    d->eraseFromParent();
  }
}

static void module_md(Module &m) {
  LLVMContext &c = m.getContext();
  const char *limits[][2] = { { "air.max_device_buffers", "31" }, { "air.max_constant_buffers", "31" },
                              { "air.max_threadgroup_buffers", "31" }, { "air.max_textures", "128" },
                              { "air.max_read_write_textures", "8" }, { "air.max_samplers", "16" } };

  for (auto &l : limits) {
    m.addModuleFlag(Module::Max, l[0], (uint32_t)atoi(l[1]));
  }
  m.getOrInsertNamedMetadata("air.version")->addOperand(Md(c).i(2).i(7).i(0).get());
  m.getOrInsertNamedMetadata("air.language_version")->addOperand(Md(c).s("Metal").i(3).i(2).i(0).get());
  NamedMDNode *co = m.getOrInsertNamedMetadata("air.compile_options");
  co->addOperand(Md(c).s("air.compile.denorms_disable").get());
  co->addOperand(Md(c).s("air.compile.fast_math_enable").get());
  co->addOperand(Md(c).s("air.compile.framebuffer_fetch_enable").get());
}

static void put(std::string &b, uint64_t v, unsigned n) {
  for (unsigned i = 0; i < n; i++)
    b += (char)(v >> 8 * i);
}

// a metallib the way pdmtl_lib_load_data reads one: the function list offset at 0x18, the bitcode section at
// 0x48, one entry per function with NAME, MDSZ and OFFT tags, all sharing the module's bitcode
static bool write_mtlb(const std::string &path, Module &m, const std::vector<Entry> &es) {
  SmallVector<char, 0> bc;
  raw_svector_ostream bs(bc);
  std::string list, file;
  std::error_code ec;

  WriteBitcodeToFile(m, bs);
  put(list, es.size(), 4);
  for (const Entry &e : es) {
    std::string t;
    t += "NAME";
    put(t, e.name.size() + 1, 2);
    t += e.name;
    t += '\0';
    t += "MDSZ";
    put(t, 8, 2);
    put(t, bc.size(), 8);
    t += "OFFT";
    put(t, 24, 2);
    put(t, 0, 8);
    put(t, 0, 8);
    put(t, 0, 8);
    t += "ENDT";
    put(list, t.size() + 4, 4);
    list += t;
  }
  uint64_t flist = 0x58, bcoff = flist + list.size();
  file = "MTLB";
  file.resize(0x18, '\0');
  put(file, flist, 8);
  put(file, list.size(), 8);
  file.resize(0x48, '\0');
  put(file, bcoff, 8);
  put(file, bc.size(), 8);
  file += list;
  file.append(bc.data(), bc.size());

  raw_fd_ostream o(path, ec, sys::fs::OF_None);
  if (ec)
    return false;
  o << file;
  return true;
}

int main(int argc, char **argv) {
  std::string in, out, ll, lib, mtlb, entry;
  LLVMContext c;
  SMDiagnostic diag;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-o" && i + 1 < argc)
      out = argv[++i];
    else if (a == "-S" && i + 1 < argc)
      ll = argv[++i];
    else if (a == "-m" && i + 1 < argc)
      lib = argv[++i];
    else if (a == "-L" && i + 1 < argc)
      mtlb = argv[++i];
    else if (a == "-e" && i + 1 < argc)
      entry = argv[++i];
    else
      in = a;
  }
  if (in.empty())
    fail("usage: airify IN.ll [-o OUT.bc] [-S OUT.ll] [-m OUT.metallib -e ENTRY] [-L OUT.metallib]");

  std::unique_ptr<Module> m = parseIRFile(in, diag, c);
  if (!m) {
    diag.print("airify", errs());
    return 1;
  }

  std::vector<Entry> es = read_entries(*m);
  if (es.empty())
    fail("no vertex, fragment or kernel function");

  // spir calling conventions out, and every function but the entries inlined away
  for (Function &f : *m) {
    f.setCallingConv(CallingConv::C);
    for (Instruction &in : instructions(f)) {
      if (auto *cb = dyn_cast<CallBase>(&in))
        cb->setCallingConv(CallingConv::C);
    }
  }
  fix_sret_calls(*m);

  std::vector<Function *> entries;
  for (Entry &e : es) {
    entries.push_back(build_entry(*m, e));
  }
  for (Function &f : *m) {
    if (f.isDeclaration() || std::find(entries.begin(), entries.end(), &f) != entries.end())
      continue;
    f.setLinkage(GlobalValue::InternalLinkage);
    f.setComdat(nullptr);
    f.removeFnAttr(Attribute::NoInline);
    f.removeFnAttr(Attribute::OptimizeNone);
    f.addFnAttr(Attribute::AlwaysInline);
  }
  for (GlobalVariable &g : m->globals()) {
    g.setComdat(nullptr);
    if (!g.isDeclaration() && !g.getName().starts_with("llvm."))
      g.setLinkage(GlobalValue::InternalLinkage);
  }
  m->getComdatSymbolTable().clear();

  run(*m, "always-inline,globaldce");
  constant_globals(*m);
  resolve_casts(*m, false);
  run(*m, "O2");
  resolve_casts(*m, true);
  run(*m, "function(sroa,instcombine<no-verify-fixpoint>,simplifycfg,adce),globaldce");
  resolve_casts(*m, true);
  lower_intrinsics(*m);
  run(*m, "function(instcombine<no-verify-fixpoint>,simplifycfg,adce),globaldce");
  lower_intrinsics(*m);
  lower_converts(*m);
  const_samplers(*m);
  function_constants(*m);

  // AIR's target, attributes that only meant something to spir, and the module metadata
  for (Function &f : *m) {
    f.removeFnAttr("target-features");
    f.removeFnAttr("target-cpu");
    f.removeFnAttr("uniform-work-group-size");
  }
  for (const char *n : { "opencl.ocl.version", "opencl.cxx.version", "opencl.spir.version" }) {
    if (NamedMDNode *nm = m->getNamedMetadata(n))
      m->eraseNamedMetadata(nm);
  }
  m->setTargetTriple(Triple(air_triple));
  m->setDataLayout(air_layout);
  module_md(*m);

  for (Function &f : *m) {
    for (Instruction &in : instructions(f)) {
      for (Value *op : in.operands()) {
        if (op->getType()->isPointerTy() && op->getType()->getPointerAddressSpace() == AS_GENERIC)
          errs() << "airify: " << f.getName() << ": generic pointer left: " << in << "\n";
      }
    }
  }
  if (verifyModule(*m, &errs()))
    fail("module does not verify");

  std::error_code ec;
  if (!ll.empty()) {
    raw_fd_ostream o(ll, ec, sys::fs::OF_Text);
    m->print(o, nullptr);
  }
  if (!out.empty()) {
    raw_fd_ostream o(out, ec, sys::fs::OF_None);
    WriteBitcodeToFile(*m, o);
  }
  // a one function library the way air2spv and airjitd find functions: a NAME tag, then the bitcode
  if (!lib.empty()) {
    SmallVector<char, 0> bc;
    raw_svector_ostream bs(bc);
    WriteBitcodeToFile(*m, bs);
    if (entry.empty())
      entry = es[0].name;
    raw_fd_ostream o(lib, ec, sys::fs::OF_None);
    uint16_t n = (uint16_t)(entry.size() + 1);
    o << "NAME" << (char)(n & 0xff) << (char)(n >> 8) << entry << '\0';
    o.write(bc.data(), bc.size());
  }
  if (!mtlb.empty() && !write_mtlb(mtlb, *m, es))
    fail("cannot write " + mtlb);
  return 0;
}
