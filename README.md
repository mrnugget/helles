# Helles

A prototypical web server in C, using a master-worker architecture, leveraging
Unix pipes as IPC mechanism.

At the moment it doesn't really work as a HTTP server, since it has a standard
reply for every request.

**Do not use this in production!**

## Building it

Use this to build it:

```
make all
```

Or this, to build it with debug information output:

```
make debug
```

## Running it

Build it. Then run this:

```
./helles 3333
```

This will boot helles, listening on port 3333.

## Change the number of workers

Change `N_WORKERS` in `main.h`

## Licencse

All code except `src/http_parser.c` and `src/http_parser.h`: MIT

The http_parser is under copyright of Joyent: [http_parser](https://github.com/joyent/http-parser)
