Put .mod / .xm / .s3m / .it files in this folder to play them offline.
On the PS5 this folder is /data/homebrew/PPSA01153/music/.

Add every file name to index.txt as well, one per line. The app cannot list
this folder by itself: a title sandbox is allowed to open a file but not to
read a directory, so index.txt is how it learns what is here.

Example index.txt:

    my-favourite.xm
    another-tune.mod

Circle opens the file list in the app, L1 and R1 step through it.
