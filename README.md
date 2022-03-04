# betterfpk
SystemC FPK-Tool with ZLC compression. Not stable or complete, but does its job (for now).
Only tested with TsuyokissNext so far.

## Usage
```
Usage: betterfpk.exe [options] <input>
Options should be from the following list:

Modes:
  -e, --extract       extract PFK archive (default)
  -p, --pack          pack FPK archive
  -l, --list          only list files in the archive

Compressions:
  -z, --zlc           enable ZLC compression (default)
  -Z, --Zlc           disable ZLC compression
  -r, --rle           enable RLE compression
  -R, --Rle           disable ZLC compression (default)

Packing options:
  -t, --threads <n>   number of threads to use while compression (default: #system threads)
  -k, --key <key>     the key to use while obfuscating (default: 0)

General options:
  -h, --help          show this help message and exit
  -o, --output        set the output path
  -v, --verbose       print detailed information while processing
```
