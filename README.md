# rin

A tag-centric data browser and management application; the reflexive indexer.

When somebody wants to interact with files on their computer, it has traditionally been the file browser that
has facilitated that end. Most file browsers today are stable, mature, and feature-rich. Most users would have
no issue with the file browsers of today for basic purposes. However, when the need arises for a more in-depth,
and thorough system for organizing files, the traditional filesystem and file browser soon fall short.

Say you have a picture of a cat that you have just downloaded and want to sort said picture into your files somewhere.
If you practice some modicum of organizational habit, you likely have one or several options that present themselves:

- Sort into the Pictures/cats folder
- Sort into the Pictures/animals folder
- Sort into the Pictures/pets folder

All of these seem like equally valid options. A cat is an animal, who may or may not be a pet. Most would likely be
satisfied with moving the cat picture into *one* of these folders and calling it a day.

For arguments sake, let's pick the Pictures/cats folder. Now say, being the cat connoisseur that you are, you have downloaded 
another picture of a cat, but this time the cat is wearing a party hat. This picture is a rare, once-in-a-lifetime shot, it evokes
several emotions within you and you forsee yourself coming back to view the picture several times in the future. You want to be able to
get to it quickly. You proceed to move it to the Pictures/cats folder on your filesystem, satisfied with the work you've done for the day.

You then notice that your cats folder contains 107,245 other pictures of cats. How are we getting to our party hat cat in any reasonable time
if we have to swim through this ocean of cat pictures? We might try several things:

- rename the file to *super_important_partyhat_cat.png* then remember to search for it with those keywords
- create a symbolic link to the picture and place the link somewhere easy to get to, like the desktop
- create a new folder (inside of Pictures/cats or Pictures/) just for party hat cats

These are all okay solutions if you don't particularly care for the messiness or overhead of some of them. Who knows how long it might take to
search through 100k+ files, or how messy your desktop will end up if you keep making symbolic links to cat pictures. Creating a new folder seems,
to me at least, like the cleanest and simplest solution of the three, but that one isn't too great either. Where do we create the folder? A sub-folder
such as Pictures/cats/partyhat-wearing-cats? Maybe Pictures/partyhat-cats? What if the picture has a dog in it? Now our whole world has turned upside down.
Do we copy the picture to two different folders: Pictures/cats and Pictures/dogs? Create multiple symlinks? Rename multiple files?

It quickly becomes apparent that the traditional directory-hierarchy structure present on today's filesystems is woefully lacking when the goal is
fine-grained, efficient, and accurate categorization and retrieval of files.

This project was started with the goal of providing a solution to this problem first and foremost. It has also served as a means of personal learning and
experience building. As of writing this, it has been a little over a year since I originally began working on this project from the ground up.

While it is far from what I had envisioned for the prototype, I made it a personal goal to publish a working build of this project by June 2026. Though
there are many flaws, I am proud of what I have managed to accomplish in this past year and have no intention of stopping now. Please look forward to updates
that I will be making to this repository as time goes on.

Please note that this is not ready for prime-time use -at all-. Do not entrust your data to this application if you are afraid to lose it (you should be making backups
anyway). This is a proof-of-concept prototype meant to demonstrate (mostly to myself) what I am capable of and all that I have learned so far.

## Building
While MacOS and Windows builds are possible in theory, I have not tested this on those platforms and make no guarantee that they will successfully compile
for those operating systems at this time. With that being said, a build for MacOS and Linux-based operating systems can be managed using Nix.

### Using Nix
This repository contains a flake.nix and flake.lock file. Allowing Nix users to easily build Rin with just the repository's URL:
```
nix build https://github.com/reivelte/rin
```

Alternatively, you can enter a Nix development shell and use the included build.py python script to build a debug verison of the project:
```
git clone https://github.com/reivelte/rin
cd rin
nix develop
build   # <- this alias will be created in the development shell and invokes build.py for you
```