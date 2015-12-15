##U8glib package 

Ported from https://github.com/olikraus/u8glib


Here is black magic with ImageMagic package  for image conversion:



```bash
#!/bin/bash

mkdir out

for icon in *png; do
    convert $icon -depth 1 ./out/$(basename $icon .png)_1bpp.png
    if [[ "$icon" == *"black"* ]]; then
        convert ./out/$(basename $icon .png)_1bpp.png -background white -alpha Background ./out/$(basename $icon .png)_1bpp.xbm
    else
        convert ./out/$(basename $icon .png)_1bpp.png -background black -alpha Background ./out/$(basename $icon .png)_1bpp.xbm
    fi
    cat ./out/$(basename $icon .png)_1bpp.xbm | tr '\n' ' '  | tr -d " " |sed  -e s'#^.*{##g'  | sed s'#,}##' |sed s'/;//' | xxd -r -p > ./out/$(basename $icon .png).xbm.mono
done

rm out/*png out/*xbm
```
Convert all the png in the current folder and put resulting .mono to ./out

The convert binary  is a part of ImageMagic package. You need to install it.

```
sudo apt-get install imagemagick
```
for Debian/Ubuntu.