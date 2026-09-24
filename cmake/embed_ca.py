"""Embed the pinned Mozilla roots as DER, omitting PEM text/base64 overhead."""
import base64
import pathlib
import re
import sys

source = pathlib.Path(sys.argv[1]).read_bytes()
certificates = [base64.b64decode(re.sub(rb'\s', b'', text), validate=True)
                for text in re.findall(rb'-----BEGIN CERTIFICATE-----(.*?)-----END CERTIFICATE-----', source, re.S)]
if len(certificates) != 121:
    raise SystemExit('Unexpected pinned CA certificate count')
data = b''.join(certificates)
lines = ['/* Mozilla roots: MPL-2.0; generated from the pinned curl bundle. */',
         'static const unsigned char mtm_ca_bundle[] = {']
lines += [','.join(f'0x{b:02x}' for b in data[i:i+24])+',' for i in range(0, len(data), 24)]
lines += ['};', 'static const unsigned short mtm_ca_lengths[] = {',
          ','.join(str(len(cert)) for cert in certificates), '};']
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines)+'\n', encoding='ascii')
