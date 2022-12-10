# apple-xbs-support/clang_device.mk
# Redirect to the 'clang' base project.

include apple-xbs-support/clang.mk

# clang_device is configured to build using the 'install-cross' make target.
# in practice it can just use the clang target.
install-cross: clang
