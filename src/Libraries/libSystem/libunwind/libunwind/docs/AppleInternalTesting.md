Apple Internal Testing for libunwind
====================================

## Open source testing

To be setup: <rdar://problem/57362849> Setup CI for libunwind

libunwind currently has very few lit tests. You can run them manually by:
```
$ cmake -GNinja ... -DLLVM_ENABLE_PROJECTS="libunwind" .. $PATH_TO_LLVM_PROJECT/llvm
$ ninja check-unwind
```


## PR Testing on stash

llvm-project on stash has PR testing setup for libunwind on ATP. The PR job
is setup under: ``"Dev Tools" -> "libunwind"``

You can trigger the test directly on a stash PR by commenting:

"@sweci [please] build libunwind for [TRAIN]"

Currently, PR Testing only checks if libunwind builds successfully in XBS,
no runtime tests are performed.


## BATS testing

You can test libunwind using BATS. Here is an example:

```
$ /SWE/CoreOS/Tools/bin/bats build --build CurrentGolden+ --project libunwind:$BRANCH --no-base-tag --no-radar -t test_dyld --dependents=soft --device Skylake_Broadwell_Haswell_OneRandom,H10P-OneRandom,H11_OneRandom,watch-m9-one-random,armv7k,atv,BridgeOSDefault-OneRandom -u $OD_USER
```

* CurrentGolden+ will build for all the downstream train for Golden, including iOS, tvOS, bridgeOS, etc.
* `--no-radar` can be replaced with `-r $RADAR_NUMBER`
* `--dependents=soft` will force BATS build important downstream clients, like dyld
* `-t` can specify tests to run after the build finishes. `test_dyld` is a good test since dyld2 uses libunwind extensively. The other tests that are good to run: `nightly` for nightly test, `guardian_validation_ring3` is another good one.
* `--device` is a long string of device that currently use to pick up one device for each architecture. It is important to test libunwind on all architectures.

A good way to run BATS against PR on stash is to specify: `--project libunwind:refs/pull-requests/$PR_NUMBER/merge`


## SWB Testing

You can also run SWB with libunwind submission.
