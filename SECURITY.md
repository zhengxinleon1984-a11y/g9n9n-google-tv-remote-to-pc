# Security Policy

## Do not publish device backups

A complete 16MB flash backup can contain:

- Wi-Fi SSID and password
- NVS key mappings
- BLE bond data
- device-specific identifiers

Never attach a full flash backup to an issue, release, or pull request.

## Do not publish credentials

Do not commit:

- GitHub tokens
- API keys
- Wi-Fi credentials
- private MAC addresses
- serial logs containing personal data

The repository ignores `.env`, runtime logs, local companion configuration,
recorded audio, and firmware binaries.

## Reporting

Report security issues privately to the repository maintainer. Do not open a
public issue containing credentials or a full device backup.
