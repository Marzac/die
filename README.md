# DIE Engine & Waller

Hello, I'm Fred ;-)

You probably know me from my synthesizer work. But I always wanted to be a game dev, and I'm not getting any younger, so here we are!

This repository is my attempt to give the community of hobby developers a basic but fully functional toolkit for building 2.5D games.

## What's inside

- **DIE** : the renderer and runtime, in `common/engine`.
- **Waller** : the map editor, in `editor/`.  

Waller is the real strength of the toolkit: a full Qt-based level editor for building maps for the engine.

## The renderer

The renderer is sorta unique. It's software-based and works a bit like a futuristic raycaster,
I'm writing a paper about it to describe its inner in more details.

Also, it's not yet fully portable, since a lot of the core rendering code relies on SSE4 arithmetic.
That said, it shouldn't be too hard to make a portable version with the tools available today.

## A weird codebase

This codebase ranges from pure SSE intrinsics for rasterizing, to fairly advanced C++ with seamless
multithreading. It's a strange mix, of modern and retro tech, what I personally love and hopefully
a good mix!

## Building

- Waller (and the engine) is built on **Qt** (Community Edition, **Qt 6** or newer).
  Just install the Qt SDK and open `editor/waller.pro`.
- For now this is **Windows only**. Linux compatibility will come reasonably soon.

## Documentation

The Doxygen-generated API reference lives in [`documentation/`](documentation/index.html) and can be
viewed online via [htmlpreview](https://htmlpreview.github.io/?https://github.com/Marzac/die/blob/main/documentation/index.html).

To regenerate it yourself, run `doxygen Doxyfile` from the `documentation/` folder.

## License

This project is **MIT licensed**
=> I don't expect anything in return. I'm using it myself for my in-development indy game.

Feel free to fork it, have a go, build your own game with it, and have a blast.
Just don't forget to credit my work ;-)

## A personal note

Never forget this is a hobby project for me. It's over two years of spare-time work,
mostly to disconnect from the harder sides of life.

Hope you'll love it and have fun.

Take care, greetings from Bonn / Germany.

Fred.
