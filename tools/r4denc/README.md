This is the R4 menu encoder from [devkitPro/dstools](https://github.com/devkitPro/dstools/tree/807da5d15033790f9cd15b7cc1172a79cb5ac4bc/src/r4denc),
commit `807da5d15033790f9cd15b7cc1172a79cb5ac4bc`.
Copyright 2007 yasu and 2009 WinterMute; distributed under GPL version 2
(see LICENSE). It is a separate host utility, used only to package the R4
and Ace3DS+ autoboot binaries. BlocksDS does not ship it.

Local changes: process only the bytes read in the last sector, and propagate
read, write and close failures to make.
