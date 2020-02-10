###############################################################################
# This is provided for the 'demo' target of the Makefile.
#
# The version of 'html2text' that is available on Darwin lacks UTF-8 support.
# However, the version provided by the Fedora linux distribution has been
# patched with UTF-8 support and other enhancements that make it quite useful.
# There might be other / better html converters out there, but 'html2text' has
# a relatively simple interface and its dependencies are self-contained.
#
# The files in this directory were extracted from the source RPM package
# 'html2text-1.3.2a-14.fc21.src.rpm' and are provided with the original license
# from the package.  Since rpm packages are not natively supported on Darwin
# (and some other architectures), this seems like the easiest solution.
#
# Sadly, this simple tool disappeared from the Fedora distros.  There _seems_
# to be a replacement, python2-html2text or python3-html2text but I don't
# know if its similar name really means identical functionality for my needs.
#
