# pngass

This is a command-line tool to render text onto one or more input PNG images.
The rendered image can be saved as a png or jpg file.
The input text can be processed using some built-in functions, or <code>/bin/sed</code> script snippets.

## Simplified Description

A typical WN from [Death March kara Hajimaru Isekai Kyousoukyoko, Volume 1, Chapter 1](https://www.sousetsuka.com/2015/01/death-march-kara-hajimaru-isekai.html)
looks like --
![sample](/SAMPLEs/Death_March_html_page.png)
<br>and with its text rendered on the PNG background image, look like
<br>(the first image from <code>make demo</code>) --
![sample](/SAMPLEs/p0001-rendered.jpg)

The background image can be <i>anything</i>, but my favorite thing is to use a bunch of
snapshots from the Anime series (if it has been adapted).

Right now, the background image(s) must be all the same size and for best results,
should be the size of the viewing device (TV or whatever).
Turn off subtitles if you do this as the subtitle text on the background image
will make the WN's text difficult to read.

## Requirements

The <code>pngass</code> utility is primarily built on a Fedora linux system, with a macOS port as a secondary platform target.
There isn't any particular reason why this code shouldn't port to windows (with cygwin?), but there's no provision for that as of yet.

* About 50 Meg of total disk space in the cloned directory (not a lot).
* A development system is needed with various libraries and their development packages including:
  * libjpeg
  * libpng
  * libass
  * there may be others needed depending on the system’s base installation.
* Standard shell tools including sed, grep, ImageMagick (for convert), wget, unzip, and a few others.
* macOS requires some additional packages managed through <code>brew</code> install (installed roughly in this order):
  * gnulib
  * coreutils
  * gnu-sed
  * libass
  * mkvtoolnix
  * jpeg
  * imagemagick
  * wget (some WN sites may require wget >= 1.14)
  * xv image viewer (not available through <code>brew</code> and is completely optional, but very nice to have).
  * When brew install is executed, other dependencies are added as well.<br>(I think that is all of them :smile:.)

## Get Started With <code><font color="green">make demo</code>

Until more complete documentation is added, the simplest way to get started is to type <code>make demo</code>.<br>
The demo requires internet access and simply:
* uses wget to get a chapter from a popular translated WN,
  * performs some very simple edits to remove the html from the text,
* gets some 1920x1080 background imagges,
* converts those images to a format suitable for reading text (lightens them),
* and executes <code>pngass</code> to apply the text to the sample images.

The rendered images are located in the folder <code>./JPGs</code>.


## High-level Overview of Some Built-in <code>pngass</code> Features
### Text re-formatting
Some of the built-in features include adding two spaces after a sentence end, or converting 
quote-pairs to matching left-right quotes (single or double).
<br>Example :arrow_right:

<blockquote>He said, "Wow, that's really great!"
<br>:arrow_right:<br>
He said, <font color="lightblue">“</font>Wow, that's really great!<font color="lightblue">”</font><br>
</blockquote>

Nested quote pairs are matched as well (I call these quote pairs "unison quotes" and are used when more than one speaker says the same thing).  These are pretty common in Japanese WN translations that I have seen.


### Stream editing
The <code>/bin/sed</code> feature can process the input text through various regex scripts to:
1. correct common translation spelling errors,
1. apply libass character attributes to selected strings including:
  * apply bold, italic, or any libass supported attribute,
  * change font, font size, or font colour;
1. add decorative font characters at particular places in the text (see demo pictures for examples).

### Backend engine repair

For whatever reason, some WN sites occasionally have duplicate groups of text (this may be related to ad placement in the body of the text, but that's just a guess).
These duplicates do <strong>not</strong> make linguistic sense, but appear on the html page in the browser; so they are not added by any process here.
For some sites, they happen often enough to be really annoying, so <code>pngass</code> can delete these duplicate if desired.



<p style="color:red">:boom: This is still in "beta", so there are likely <i>many</i> bugs, incomplete features, and missing documentation.</p>


(My first github project, so files will slowly be added over time.)
