# Security Policy

## Supported versions

| Version | Supported |
| ------- | --------- |
| latest main | yes |
| 1.x releases | yes |
| anything older | no |

## Reporting a vulnerability

Report vulnerabilities privately through GitHub's security advisories. Open the Security tab
of this repository and choose "Report a vulnerability". Please do not open a public issue
for something exploitable.

This is a spare-time project with one maintainer, so the honest promise is best effort. You
can expect an acknowledgment within a week and a fix as fast as severity warrants.

## Scope

The most plausible attack surface is file parsing. The example player and the web player
read untrusted DRO, HSC, and IMF files, and the decoders are written to treat their input
with suspicion. Malformed songs should produce silence or a polite refusal, never memory
corruption. If you find an input that does otherwise, that is exactly the report we want.

The synthesis core itself consumes register writes and produces samples, which leaves it
little room for mischief. Vulnerabilities in miniaudio belong upstream at
https://github.com/mackron/miniaudio, though a note here is welcome if the player is
affected.
