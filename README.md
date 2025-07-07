# betterfpk
SystemC FPK-Tool with ZLC compression. Not stable or complete, but does its job (for now).
Only tested with TsuyokissNext so far.

You might be wondering: why is this crappy piece of code called "*better*fpk"? Well, because I once made an even worse ZLC compressor (never published it though...).

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
  -R, --Rle           disable RLE compression (default)

Packing options:
  -t, --threads <n>   number of threads to use while compression (default: #system threads)
  -k, --key <key>     the key to use while obfuscating (default: 0)

General options:
  -h, --help          show this help message and exit
  -o, --output        set the output path
  -ver --version      set the extract/repack version (default: 2)
  -v, --verbose       print detailed information while processing
```

### Examples
Extracting: 
```betterfpk.exe --extract -o cg_extracted cg.fpk```
```betterfpk.exe --extract --version 4 -o data_extracted data.fpk```

Repacking:
```betterfpk.exe --pack -o cg_modified.pak folder/with/modified/cgs```
```betterfpk.exe --pack --version 4 -o data_modified.pak folder/with/modified/data```
