#!/usr/bin/env python3
# builds trustd's Certificates.bundle from a PEM bundle, certsIndex maps each root's normalized subject
# (createNormalizedX501Name) to its certsTable record, escrow lists get one placeholder certificate
import base64
import hashlib
import plistlib
import struct
import sys
from pathlib import Path


def read_tlv(data, pos):
    tag = data[pos]
    length = data[pos + 1]
    header = 2
    if length & 0x80:
        count = length & 0x7F
        length = int.from_bytes(data[pos + 2:pos + 2 + count], "big")
        header += count
    return tag, pos + header, length


def encode_length(length):
    if length < 0x80:
        return bytes([length])
    raw = length.to_bytes((length.bit_length() + 7) // 8, "big")
    return bytes([0x80 | len(raw)]) + raw


def encode(tag, content):
    return bytes([tag]) + encode_length(len(content)) + content


def children(data, start, end):
    pos = start
    while pos < end:
        tag, content, length = read_tlv(data, pos)
        yield tag, data[pos:content + length], data[content:content + length]
        pos = content + length


def normalize_printable(value):
    # upper case a to z, runs of spaces and tabs become one space, no leading or trailing space
    out = bytearray()
    last_blank = False
    for ch in value:
        if ch in (0x20, 0x09):
            if not last_blank and out:
                out.append(0x20)
            last_blank = True
        else:
            last_blank = False
            out.append(ch - 0x20 if 0x61 <= ch <= 0x7A else ch)
    if last_blank and out and out[-1] == 0x20:
        out.pop()
    return bytes(out)


def normalized_name_content(name_content):
    # the RDN sets in order, each value re-encoded only if it is a PrintableString
    result = bytearray()
    for rdn_tag, _, rdn_content in children(name_content, 0, len(name_content)):
        atvs = bytearray()
        for atv_tag, _, atv_content in children(rdn_content, 0, len(rdn_content)):
            parts = list(children(atv_content, 0, len(atv_content)))
            oid_tlv = parts[0][1]
            value_tag, value_tlv, value = parts[1]
            if value_tag == 0x13:
                value_tlv = encode(0x13, normalize_printable(value))
            atvs += encode(atv_tag, oid_tlv + value_tlv + b"".join(p[1] for p in parts[2:]))
        result += encode(rdn_tag, bytes(atvs))
    return bytes(result)


def subject_name_content(der):
    _, cert_content, cert_length = read_tlv(der, 0)
    tbs = next(children(der, cert_content, cert_content + cert_length))[2]
    fields = list(children(tbs, 0, len(tbs)))
    # an explicit version comes first as [0]
    if fields[0][0] == 0xA0:
        fields = fields[1:]
    # serial, signature algorithm, issuer, validity, subject
    return fields[4][2]


def pem_certificates(text):
    blocks = text.split("-----BEGIN CERTIFICATE-----")[1:]
    for block in blocks:
        body = block.split("-----END CERTIFICATE-----")[0]
        yield base64.b64decode("".join(body.split()))


def main(pem_path, bundle_path, version, escrow_path):
    certificates = list(dict.fromkeys(pem_certificates(Path(pem_path).read_text())))
    table = bytearray()
    index = []
    for der in certificates:
        digest = hashlib.sha1(normalized_name_content(subject_name_content(der))).digest()
        index.append((digest, len(table)))
        table += struct.pack("<II", 8 + len(der), len(der)) + der
    index.sort()

    contents = Path(bundle_path) / "Contents"
    resources = contents / "Resources"
    resources.mkdir(parents=True, exist_ok=True)
    (resources / "certsTable.data").write_bytes(table)
    (resources / "certsIndex.data").write_bytes(b"".join(d + struct.pack("<I", o) for d, o in index))

    def plist(name, value):
        (resources / name).write_bytes(plistlib.dumps(value, fmt=plistlib.FMT_XML))

    # no keys are blocked or gray listed and no roots are marked for EV, Mozilla's bundle carries neither
    plist("Blocked.plist", [])
    plist("GrayListedKeys.plist", [])
    plist("EVRoots.plist", {})
    escrow = Path(escrow_path).read_bytes()
    plist("AppleESCertificates.plist", {"ProductionEscrowKey": [escrow], "ProductionPCSEscrowKey": [escrow]})
    plist("AssetVersion.plist", {"VersionNumber": version, "MobileAssetContentVersion": version})
    # the pinning list is its version followed by pinning rules, and there are none here
    plist("CertificatePinning.plist", [version])
    (contents / "Info.plist").write_bytes(plistlib.dumps({
        "CFBundleIdentifier": "com.apple.security.certificates",
        "CFBundleName": "Certificates",
        "CFBundlePackageType": "BNDL",
        "CFBundleShortVersionString": str(version),
        "CFBundleVersion": str(version),
    }, fmt=plistlib.FMT_XML))
    print(f"{len(certificates)} roots")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4])
