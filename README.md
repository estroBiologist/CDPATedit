# CDPATedit

> NOTE: This repository is publicly available for portfolio purposes, although CHORDIOID itself isn't. I want to work on making this editor more independent in the future, but as of right now it's pretty useless without the necessary Godot project structure. For an example of what the editor can do in action, please refer to this video I haphazardly threw together!

[<img src="https://media.discordapp.net/attachments/846781793834106902/1088780190244671488/Photoshop_jOUPFrsThV.png?width=919&height=517" width="50%">](https://www.youtube.com/watch?v=9CJPhTq5m-4)

Editor for CHORDIOID's beatmap format, `.cdpat`.

## Downloading

Head over to [Releases](https://gitlab.com/estroBiologist/CDPATedit/-/releases)! Assuming I've remembered to update it regularly, that is.

## Compiling from source

This repo is a Visual Studio 2022 project, and uses [vcpkg](https://vcpkg.io/en/index.html) for its dependencies.

Or rather, its dependency. The only external dependency for this project is SFML 2.5.1. Once that's all installed, it should compile with no difficulties.

## Usage

Usage instructions for the actual editor are included (`Help > Show Help Panel`), but first you'll need to locate the CHORDIOID project folder.

When the editor first opens, all the windows will be tiny, and stacked on top of each other. Sorry. Just drag 'em to the right and arrange 'em however you like.

Then use either the `File` menu or the `Configuration` panel, locate your CHORDIOID folder. You'll want to open the `project.godot` file. If everything went well, the `Pattern` panel should now list the existing patterns in your CHORDIOID folder! Click on one to open it, or continue with the blank file you have open by default.

The `Help` panel should teach you the basic editor controls. Good luck!
