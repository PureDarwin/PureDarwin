{ runCommand
, binutils
, sbsigntool
, mtools
, shim
, shimIa32
}:

# the Secure Boot twin of a PureDarwin image, shim as EFI/BOOT/BOOTX64.EFI loads the xnu-loader as grubx64.efi
# the first boot shows MokManager, which enrolls EFI/BOOT/PUREDARWIN.cer from the disk
{ image, key }:

runCommand "${image.name}-secureboot" {
  nativeBuildInputs = [ binutils sbsigntool mtools ];
} ''
  if [ ! -r ${key}/MOK.key ] || [ ! -r ${key}/MOK.pem ] || [ ! -r ${key}/MOK.cer ]; then
    echo "no PureDarwin MOK key, see nix/secureboot/no-key/README" >&2
    exit 1
  fi
  mkdir -p $out
  for img in ${image}/*.img; do
    name=$(basename "$img" .img)
    out_img=$out/$name-sb.img
    cp "$img" "$out_img"
    chmod u+w "$out_img"
    # the ESP is the first partition, at 1 MiB in every PureDarwin image
    esp="$out_img@@1048576"
    mcopy -n -i "$esp" ::/EFI/BOOT/BOOTX64.EFI loader.efi

    # shim refuses an image without SBAT data, the section goes at the end of the image
    end=$(objdump -p loader.efi | awk '/^SizeOfImage/ {print $2}')
    objcopy --target=efi-app-x86_64 \
      --add-section .sbat=${../secureboot/sbat.csv} \
      --set-section-flags .sbat=contents,alloc,load,readonly,data \
      --change-section-vma .sbat=0x$end \
      loader.efi loader-sbat.efi
    sbsign --key ${key}/MOK.key --cert ${key}/MOK.pem --output grubx64.efi loader-sbat.efi
    sbverify --cert ${key}/MOK.pem grubx64.efi

    mcopy -o -i "$esp" grubx64.efi ::/EFI/BOOT/grubx64.efi
    mcopy -o -i "$esp" ${shim}/efi/shimx64.efi ::/EFI/BOOT/BOOTX64.EFI
    mcopy -o -i "$esp" ${shim}/efi/mmx64.efi ::/EFI/BOOT/mmx64.efi
    mcopy -o -i "$esp" ${key}/MOK.cer ::/EFI/BOOT/PUREDARWIN.cer
    rm -f loader.efi loader-sbat.efi grubx64.efi

    # the same chain for 32-bit uefi when the image carries BOOTIA32.EFI, through Debian's signed ia32 shim
    if mcopy -n -i "$esp" ::/EFI/BOOT/BOOTIA32.EFI loader32.efi 2>/dev/null; then
      end=$(objdump -p loader32.efi | awk '/^SizeOfImage/ {print $2}')
      objcopy --target=efi-app-ia32 \
        --add-section .sbat=${../secureboot/sbat.csv} \
        --set-section-flags .sbat=contents,alloc,load,readonly,data \
        --change-section-vma .sbat=0x$end \
        loader32.efi loader32-sbat.efi
      sbsign --key ${key}/MOK.key --cert ${key}/MOK.pem --output grubia32.efi loader32-sbat.efi
      sbverify --cert ${key}/MOK.pem grubia32.efi
      mcopy -o -i "$esp" grubia32.efi ::/EFI/BOOT/grubia32.efi
      mcopy -o -i "$esp" ${shimIa32}/efi/shimia32.efi ::/EFI/BOOT/BOOTIA32.EFI
      mcopy -o -i "$esp" ${shimIa32}/efi/mmia32.efi ::/EFI/BOOT/mmia32.efi
      rm -f loader32.efi loader32-sbat.efi grubia32.efi
    fi
  done
''
