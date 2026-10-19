# listen-vita

This is an unofficial LISTEN.moe client for the playstation vita.
The button layout is:

- Triangle to open configuration

- Cross to pause/toggle menu option

- Dpad UP or DOWN to navigate the menus

# Building
First of all, install the dependencies.

The only dependencies you'll need is VITASDK, and your favorite source code editor.
For VITASDK, set it up at https://vitasdk.org/ . The packages that listen.vita requires are already included in VITASDK. You'll also need to install Cmake, as the building process requires it.

After all that, open your terminal, and cd into the folder containing src. Once that's done, you need to execute these commands

`cmake .`

And then, execute

`make`

If you did all of this correctly, you should get a .vpk file. This .vpk file is the application to install to your playstation vita. Also, do not use a Vita3K to run the .vpk file. This is because libcurl returns CURLE_SSL_CONNECT_ERROR, and this does not happen on real hardware. The actual underlying reason has not been investigated. 

