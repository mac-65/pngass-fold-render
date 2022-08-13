#! /bin/bash

shopt -s lastpipe ; # Needed for 'while read XXX ; do' loops

###############################################################################
# At some point I'd like to incorporate this logic as command line parameters.
#
if false ; then
  cd /avm1/NO_RSYNC/MVs ;
  cd 'OUT DIR' ; ls *.mp4 | while read yy ; do echo -n "${yy} ." ; set +x ;
      if [ -s "/run/media/${USER}/2TBBlue3/MVs/${yy}" ] ; then
        TIMESTAMP_SRC=$(stat -c '%y' "${yy}" | cut -c 1-23) ;
        TIMESTAMP_DST=$(stat -c '%y' "/run/media/${USER}/2TBBlue3/MVs/${yy}" | cut -c 1-23) ;
        echo -n '.' ;
        if [[ $? -ne 0 || "${TIMESTAMP_SRC}" > "${TIMESTAMP_DST}" ]] ; then
          /bin/cp -p "${yy}" "/run/media/${USER}/2TBBlue3/MVs/${yy}" ;
          echo -n ". $(tput bold; tput setaf 5)UPDATE" ;
        else
          echo -n ".. $(tput bold; tput setaf 2)DONE" ;
        fi
      else
        echo -n '.' ;
        /bin/cp -p "${yy}" "/run/media/${USER}/2TBBlue3/MVs/${yy}" ;
        echo -n ". $(tput bold; tput setaf 3)COPY" ;
      fi ;
      tput sgr0 ; echo ;
    done
fi


###############################################################################
# TODO:
# - option to copy the audio track instead of re-encoding it as MP3
# - option to copy the video track instead of re-encoding it, or re-encode
#   the video track as something different.  For some reason, some recodes
#   don't play well on the Samsung but I can't see the different in VLC, e.g.
#   "/avm1/NO_RSYNC/MVs/3 Doors Down - It's Not My Time (CrawDad).mpg".
#


###############################################################################
# How to extract the title from an MKV container, if it's there.  I might want
# to add this as an argument to use instead of defaulting to just the input
# filename (which will be the default fallback if it's not in the video).
#
# Note, sometimes 'track_name' is used in the tracks container, for example ->
# '[PV-SAVE] Aimer - Akane Sasu [1920x732.x264.FLAC][Multi.Subs][Dual.Audio][HQ][7E6D8D2A].mkv'
#
# See 'HS_FONTs/HS-config-2022_07_Summer.sh' for example of this snippet.
#   "tracks": [
#       {
#         "codec": "AVC/H.264/MPEG-4p10",
#         "id": 0,
#         "properties": {
#           ...  ...  ...
#           "track_name": "[PV-SAVE] Aimer - Akanesasu",
#     ...  ...  ...
#
if false ; then
  ls ; read MKV_FILE ; \
    mkvmerge -i -F json "${MKV_FILE}" \
      | jq '.container' \
      | jq -r '[.properties.title]|@sh' ;

  ls ; read MKV_FILE ; \
    mkvmerge -i -F json "${MKV_FILE}" \
      | jq '.tracks[]' \
      | jq -r '[.id, .type, .properties.codec_id, .properties.track_name, .properties.language]|@sh' ;
fi


###############################################################################
###############################################################################
# I cheated 'cause I didn't know how to just get the file's extension ...
#
# https://stackoverflow.com/questions/965053/extract-filename-and-extension-in-bash
#
ATTR_OFF="`tput sgr0`" ;
ATTR_BOLD="`tput bold`" ;
ATTR_UNDL="`tput smul`" ;
ATTR_BLINK="`tput blink`" ;
ATTR_CLR_BOLD="${ATTR_OFF}${ATTR_BOLD}" ;
ATTR_RED="${ATTR_OFF}`tput setaf 1`" ;
ATTR_RED_BOLD="${ATTR_RED}${ATTR_BOLD}" ;
ATTR_GREEN="${ATTR_OFF}`tput setaf 2;`" ;
ATTR_GREEN_BOLD="${ATTR_GREEN}${ATTR_BOLD}" ;
ATTR_YELLOW="${ATTR_OFF}`tput setaf 3`" ;
ATTR_YELLOW_BOLD="${ATTR_YELLOW}${ATTR_BOLD}" ;
ATTR_BLUE="${ATTR_OFF}`tput setaf 4`" ;
ATTR_BLUE_BOLD="${ATTR_BLUE}${ATTR_BOLD}" ;
ATTR_MAGENTA="${ATTR_OFF}`tput setaf 5`" ;
ATTR_MAGENTA_BOLD="${ATTR_MAGENTA}${ATTR_BOLD}" ;
ATTR_CYAN="${ATTR_OFF}`tput setaf 6`" ;
ATTR_CYAN_BOLD="${ATTR_CYAN}${ATTR_BOLD}" ;
ATTR_BROWN="${ATTR_OFF}`tput setaf 94`" ;
ATTR_BROWN_BOLD="${ATTR_BROWN}${ATTR_BOLD}" ;
OPEN_TIC='‘' ;
ATTR_OPEN_TIC="${ATTR_CLR_BOLD}${OPEN_TIC}" ;
CLOSE_TIC='’' ;
ATTR_CLOSE_TIC="${ATTR_CLR_BOLD}${CLOSE_TIC}${ATTR_OFF}" ;

ATTR_ERROR="${ATTR_RED_BOLD}ERROR -${ATTR_OFF}" ;
ATTR_NOTE="${ATTR_OFF}`tput setaf 12`NOTE -${ATTR_OFF}";
ATTR_TOOL="${ATTR_GREEN_BOLD}" ;

G_TMP_FILE='' ;

###############################################################################
###############################################################################
# Ctl-C, signals and exit handler stuff ...
#
stty -echoctl ; # hide '^C' on the terminal output
exit_handler() {

   /bin/rm -f "${G_TMP_FILE}" ;
   echo 'EXITING' ;
}

sigint_handler() {
   { set +x ; } >/dev/null 2>&1 ;

   MY_REASON='ABORTED' ;
   if [ $# -ne 0 ] ; then
      MY_REASON="$1" ; shift ;
   fi

   exit_handler ;

   tput setaf 1 ; tput bold ;
   for yy in {01..03} ; do echo -n "${MY_REASON} BY USER  " ; done ;
   echo ; tput sgr0 ;

   exit 1 ;
}
trap 'sigint_handler' HUP INT QUIT TERM ; # Don't include 'EXIT' here...
trap 'exit_handler' EXIT ;


###############################################################################
# Required tools (many of these are probably installed by default):
#  - ffmpeg
#  - mkvtoolnix
#  - sed and pcre2
#  - coreutils -- basename, cut, head, et. al.
#  - grep, egrep
#  - dos2unix
#
MY_SCRIPT="`basename \"$0\"`" ;
DBG='.' ;
DBG='' ;
FFMPEG='/usr/local/bin/ffmpeg -y -nostdin -hide_banner' ;
FFMPEG='ffmpeg -y -nostdin -hide_banner' ;
MKVMERGE='/usr/bin/mkvmerge' ;
MKVEXTRACT='/usr/bin/mkvextract' ;
DOS2UNIX='/bin/dos2unix --force' ;
SED='/usr/bin/sed' ;

C_SCRIPT_NAME="$(basename "$0" '.sh')" ;

###############################################################################
# Some ffmpeg encoding constants.  Tweak as necessary to your preference.
# These are chosen to the "least common denominator" for the playback device
# (e.g. my TV can't playback FLAC audio or h265 video streams).
#
C_FFMPEG_CRF=20 ;
C_FFMPEG_PRESET='veryfast' ;    # Fast, used for batch script testing
C_FFMPEG_PRESET='veryslow' ;    # Good quality w/good compression
C_FFMPEG_MP3_BITS=320 ;         # We'll convert the audio track to MP3
C_SUBTITLE_OUT_DIR='./SUBs' ;   # Where to save the extracted subtitle
C_FONTS_DIR="${HOME}/.fonts" ;  # Where to save the font attachments

G_OPTION_NO_SUBS='' ;           # set to 'y' if '--no-subs' is specified
G_OPTION_NO_MODIFY_SRT='' ;     # for an SRT subtitle, don't apply any sed
                                # scripts to the generated ASS subtitle if
                                # this is set to 'y'.
                                # This flag's use should be pretty rare since
                                # ffmpeg's default conversion is pretty basic.
G_OPTION_NO_MODIFY_ASS='' ;     # TODO write_me + getopt
G_OPTION_ASS_SCRIPT=''    ;     # TODO write_me + getopt
G_OPTION_VERBOSE='' ;           # set to 'y' if '--verbose' is specified to
                                # display a little bit more status of the run
C_VIDEO_OUT_DIR='OUT DIR' ;     # the re-encoded video's save location
C_SUBTITLE_IN_DIR='IN SUBs' ;   # location for manually added subtitles

declare -a SED_SCRIPT_ARRAY=();

  #############################################################################
  # We'll encode the input video using a MP4 container.  According to
  # https://write.corbpie.com/adding-metadata-to-a-video-or-audio-file-with-ffmpeg/
  # the following metadata tags are supported in the MP4 container format:
  #   “title”, “author”, “album_artist”, “album”, “grouping”, “composer”,
  #   “year”, “track”, “comment”, “genre”, “copyright”, “description”,
  #   “synopsis”, “show”, “episode_id”, “network”, and “lyrics”.
  # It was straightforward to automate “title” and “genre” in this script, but
  # other tags could be added through some clever scripting automation as well.
  #
  # The VLC media player can show a video's metadata with some of the more
  # common tags displayed in the "Current Media Information" dialogue window.
  #
  # MPV.  Looks like metadata display can be provided by some lua scripts.
  # https://www.reddit.com/r/mpv/comments/tx1yp8/is_there_a_way_for_mpv_to_show_me_data_like_album/
  # https://github.com/vc-01/metadata-osd
  #
  # ffplay, xine, mplayer, et. al.  Dunno.
  #
  # Of course, the purpose of this script is to build a media file that is
  # watchable on a technologically lesser device than one's personal computer
  # (e.g. an old TV), so any metadata is probably limited to simple tags,
  # if at all.
  #############################################################################
  #
G_METADATA_TITLE='' ;            # '-metadata title=' the video's filename
                                 # If it's = '', we'll build it from the
                                 # filename and clean it up a bit ...
C_METADATA_GENRE='Music Video' ; # '-metadata genre=' tag default for the video
C_METADATA_COMMENT='' ;          # Search for this to modify what is included
                                 # when the default comment is built.  The
                                 # default comment includes the encoding flags,
                                 # ffmpeg's version, kernel version, and the
                                 # date the video was encoded.

###############################################################################
# We re-encode the video to h264.  BUT h264 has some size constraints that
# we have to “fix” from the input video (something about a odd number of rows
# or columns -- I don't remember the exact error message).  The easiest fix is
# to always apply the “fix” since on "correct" input videos, the fix will not
# have a negative effect.
#
# Additional video filters should be added here if needed IAW ffmpeg's syntax
# (that is, don't worry about escaping any shell special characters here).
# Notes:
#  ffmpeg applies filters in the order specified, so if a video has subtitles,
#  the subtitle filter is applied __last__.  Suppose you wanted to unsharp,
#  denoise, etc., you probably don't want those applied to the subtitles.
# https://stackoverflow.com/questions/6195872/applying-multiple-filters-at-once-with-ffmpeg
#
C_VIDEO_FILTERS='pad=width=ceil(iw/2)*2:height=ceil(ih/2)*2' ; # The "fix"
C_OUTPUT_CONTAINER='mp4' ; # Some older TVs may be able to read MKV files...

G_SUBTITLE_PATHNAME='' ; # Built by this script

###############################################################################
###############################################################################
###############################################################################
#   ######
#   #      #    #  #    #   ####   #####   #    ####   #    #   ####
#   #      #    #  ##   #  #    #    #     #   #    #  ##   #  #
#   ####   #    #  # #  #  #         #     #   #    #  # #  #   ####
#   #      #    #  #  # #  #         #     #   #    #  #  # #       #
#   #      #    #  #   ##  #    #    #     #   #    #  #   ##  #    #
#   #       ####   #    #   ####     #     #    ####   #    #   ####
#
my_usage() {
  local L_RC=$1 ; shift ;

  tput sgr0 ;
  echo "USAGE ::" ; # TODO :: add some help text
  echo -n ' ls *.flv *.avi *.mkv *.mp4 *.webm *.ts *.mpg *.vob 2>/dev/null '
  echo    '| while read yy ; do '$0' "${yy}" ; done' ;

  exit $L_RC ;
}

necho() {
  local ARG="$1" ; shift ;

  [ "${G_OPTION_VERBOSE}" = 'y' ] && echo -n "${ARG}" ;
}

vecho() {
  local ARG="$1" ; shift ;

  [ "${G_OPTION_VERBOSE}" = 'y' ] && echo "${ARG}" ;
}

###############################################################################
#
my_set_system_variables() {

  G_OPTION_NO_SUBS='' ;
  G_OPTION_VERBOSE='' ;
}

if [ $# -eq 0 ] ; then

  my_usage 2;
fi


###############################################################################
###############################################################################
###############################################################################
# Setup and use getopt for the command line arguments.
# Stuff the parsed options back into the arg[] list (quotes are essential)...
# (You know, I don't remember why I originally chose this particular method.)
#
HS_OPTIONS=`getopt -o h::vc:f:yt:q: \
    --long help::,verbose,config:,fonts-dir:,copy-to:,quality:,\
no-subs,\
no-modify-srt \
    -n "${ATTR_ERROR} ${ATTR_BLUE_BOLD}${MY_SCRIPT}${ATTR_YELLOW}" -- "$@"` ;

if [ $? != 0 ] ; then
   my_usage 1 ;
fi

my_set_system_variables ;

eval set -- "${HS_OPTIONS}" ;
while true ; do  # {
  SEP=' ' ;

  case "$1" in  # {
  --no-subs)
    G_OPTION_NO_SUBS='y' ; shift ;
    ;;
  --no-modify-srt)
    G_OPTION_NO_MODIFY_SRT='y' ; shift ;
    ;;
# --copy-title) # Attempt to copy the title from the source __video__
#   ;;
# --mux-subs)   # If there are subtitles, then also MUX them into the video
#   ;;
  -v|--verbose)
    G_OPTION_VERBOSE='y' ; shift ;
    ;;
  -h|--help)
    case "$2" in
     '')     my_usage 0 ;        ;;
     'full') my_usage 0 "'$2'" ; ;;
     *)      my_usage 1 "'$2'" ; ;;
    esac
    ;;
  --)
    shift ;
    break ;
    ;;
  *)
    echo -n "${ATTR_ERROR} fatal script error - option handler for " ;
    echo    "'${ATTR_YELLOW_BOLD}$1${ATTR_OFF}' not written!!!" ;
    echo    "${ATTR_YELLOW_BOLD}Terminating...${ATTR_OFF}" >&2 ; exit 5 ;
    ;;
  esac  # }
done ;  # }


###############################################################################
###############################################################################
###############################################################################

G_IN_FILE="$1" ; shift ;

mkdir -p "${C_SUBTITLE_OUT_DIR}" ;
if [ $? -ne 0 ] ; then
   echo "${ATTR_ERROR} Can't make the subtitle directory!" ;
   exit 1;
fi

G_IN_EXTENSION="${G_IN_FILE##*.}" ;
G_IN_BASENAME="$(basename "${G_IN_FILE}" ".${G_IN_EXTENSION}")" ;


###############################################################################
# TODO :: make these (and other) proper command line args
#
if [ $# -ne 0 ] ; then
    echo 'EXTRA ARGS' ;
    C_FFMPEG_CRF="$1" ; shift ;
    C_FFMPEG_MP3_BITS=192;
fi


###############################################################################
###############################################################################
#   ######
#   #       ####   #    #   #####   ####
#   #      #    #  ##   #     #    #
#   ####   #    #  # #  #     #     ####
#   #      #    #  #  # #     #         #
#   #      #    #  #   ##     #    #    #   ###   ###   ###
#   #       ####   #    #     #     ####    ###   ###   ###
###############################################################################
# Extract and install any font attachments that are in the INPUT file.
#
# NOTE -- NO FONT will ever be overwritten by this function.
#
extract_fonts_in_video() {

  local in_video="$1" ; shift ;
  local attachments_dir="$1" ; shift ; # Destination directory for each font attachment

  echo    "  ${ATTR_BOLD}GETTING FONTS FOR VIDEO " ;

  local attachment_line='' ;
  local attachment_list='' ;
  local pad_spaces='    ' ;

  ERR_MKVMERGE=0; # Nuttin's EASY -- font attachment identifiers, is there
                  #                  a comprehensive list somewhere ...?
                  # I have: truetype, opentype, font.ttf, and font.otf.

  local have_font_attachments='' ;
  ### TEST_CODE mkvmerge -i "${in_video}" | egrep 'XXXXXXX' \
  ${MKVMERGE} -i "${in_video}" | egrep 'truetype|opentype|font.ttf|font.otf' \
                               | while read attachment_line ; do # {

    have_font_attachments='Y' ;

      #########################################################################
      # Attachment IDs are NOT always sequential, so we need to capture each
      # attachment's ID.  Also, attachment names may have embedded SPACEs.
      #
    local attachment_ID=`echo "${attachment_line}" \
                        | ${SED} -e 's/Attachment ID \([0-9][0-9]*\):.*$/\1/'` ;
    local attachment_file=`echo "${attachment_line}" \
                        | cut -d' ' -f 11- | ${SED} -e "s/'//g"` ;

    local font_pathname="${attachments_dir}/${attachment_file}" ;

    if [ ! -f "${font_pathname}" ] ; then # {
      attachment_list="${attachment_list}${attachment_ID}:${font_pathname} " ;
      MSG="`mkvextract attachments \"${in_video}\" \"${attachment_ID}:${font_pathname}\"`" ;
      if [ $? -eq 0 ] ; then
        echo    "${MSG}" \
          | ${SED} -e "s/.*is written to \(.*\).$/${pad_spaces}<< ${ATTR_BLUE_BOLD}EXTRACTING ${ATTR_CLR_BOLD}${attachment_ID}:${attachment_file}${ATTR_OFF} to ${ATTR_CYAN_BOLD}\1${ATTR_OFF}. >>/" ;
      else
        ERR_MKVMERGE=1;
        MSG="`echo -n \"${MSG}\" | tail -1 | cut -d' ' -f2-`" ;
        echo    "${ATTR_ERROR} ${MSG}" ;
      fi
    else # }{
      echo -n "${pad_spaces}<< ${ATTR_YELLOW}SKIPPING ALREADY INSTALLED " ;
      echo    "${ATTR_MAGENTA}${attachment_ID}:${attachment_file}${ATTR_OFF}. >>" ;
    fi ; # }

  done ; # }

  if [ "${have_font_attachments}" = 'Y' ] ; then # {
    if [ "${attachment_list}" != '' -a ${ERR_MKVMERGE} -eq 0 ] ; then # {
      if [ false -a "${HS_FC_CACHE}" != '' ] ; then

        #######################################################################
        # - Note that this sctipt WILL NOT overwrite the font __file__ if it
        #   already exists in the 'attachments_dir'.
        # - Adding fonts from some videos can sometimes mess with the
        #   system-installed fonts if the 'attachments_dir' is locally visible
        #   to the 'fontconfig' system installed on your computer (solution -
        #   simply set 'attachments_dir' to something else).
        #
        echo "${ATTR_YELLOW_BOLD}Adding new FONTs using '${HS_FC_CACHE}' ${ATTR_OFF}..." ;
        ${HS_FC_CACHE} "${attachments_dir}" ;
      fi
    else  # }{
      echo -n "${ATTR_BOLD}${pad_spaces}Note - there were " ;
      [ ${ERR_MKVMERGE} -ne 0 ] && echo -n "${ATTR_RED_BOLD}errors adding fonts or " ;
      echo "${ATTR_YELLOW_BOLD}no NEW fonts found${ATTR_OFF}." ;
    fi # }

  else  # }{

      #########################################################################
      # I have seen videos (although rare) that contain a subtitle track, but
      # do not have the referenced font(s) in the subtitle as attachments.
      # It's hard to tell how these subtitles will render (either nothing at
      # all or libass will perform a font substitution with lackluster results,
      # or the font is already installed on the system and is rendered fine).
      #
      # So we'll warn the user of this situation.
      #
    echo -n "${pad_spaces}${ATTR_YELLOW}${ATTR_BLINK}" ;
    echo    "NOTE, this video file contains no FONT attachments.${ATTR_OFF}" ;
  fi # }
}


###############################################################################
###############################################################################
# extract_video_subtitle( G_IN_FILE )
# - Extracts the 1st subtitle file from the video and saves it in the current
#   directory.
# - Sets 'G_SUBTITLE_PATHNAME' with the subtitle filename suitable to pass to
#   ffmpeg (i.e., special characters to ffmpeg are properly escaped).
#
extract_video_subtitle() {  # "${G_IN_FILE}"
  local in_video="$1" ; shift ;

  #############################################################################
  # I assume that if a music video has a subtitle track, then the subtitle is
  # stylizied for the music and would NOT be a SRT or PGS subtitle.
  # Assumptions made in this section:
  # - the subtitle is Advanced SubStation Alpha (ASS) format; and
  # - we'll hardsub the first subtitle track in the video.
  #
  ${MKVMERGE} -i "${in_video}" | egrep 'subtitles' \
                               | head -1 \
                               | while read track_line ; do
    local track_ID=`echo "${track_line}" \
      | ${SED} -e 's/Track ID \([0-9][0-9]*\):.*$/\1/'` ;

      #########################################################################
      # We don't need to echo out the subtitle's save directory. [sic]
      #
      # Also, we won't overwrite the ASS file if it's already there.  This
      # allows the user to tweak the subtitle file and re-encode the video
      # using the tweaked version by removing the old re-encoded video and
      # rerunning the script.
      # TODO :: If there's an executable script with the same basename as the
      #         ASS file, then execute that script.  This would ONLY be run
      #         after the initial extraction of the subtitle file.
      #
    echo -n "  ${ATTR_CYAN_BOLD}Subtitle Track ${ATTR_CLR_BOLD}${track_ID}:" ;
    echo    "${ATTR_YELLOW}${G_IN_BASENAME}.ass ${ATTR_OFF}..." ;

    if [ ! -s "${C_SUBTITLE_OUT_DIR}/${G_IN_BASENAME}.ass" ] ; then
      ${MKVEXTRACT} tracks "${in_video}" \
             "${track_ID}:${C_SUBTITLE_OUT_DIR}/${G_IN_BASENAME}.ass" \
        >/dev/null 2>&1 ;
    fi

      #########################################################################
      # Nuttin's easy department, part 2 -- A colon/comma in the subtitle's
      # filename needs to be double-escaped and its filename __should__ be
      # prepended with './', e.g.  -vf 'subtitles=./Re\\\: Zero-01.ass:...'
      # (TODO - don't know if special handling is needed for a single quote.)
      #
    G_SUBTITLE_PATHNAME="${C_SUBTITLE_OUT_DIR}/${G_IN_BASENAME}.ass" ;
  done
}


###############################################################################
###############################################################################
# SRT subtitles are always converted to ASS subtitles using ffmpeg.
# This function provides a way to edit those subtitles, if desired.
#
# This applies some simple enhancements to the ASS script built by ffmpeg.
# Usually SRT subtitles are kinda bland and this makes them a little better.
#
apply_script_to_srt_subtitles() {  # input_pathname  output_pathname

  local L_SKIP_OPTION="$1" ; shift ;
  local ASS_SRC="$1" ; shift ;
  local ASS_DST="$1" ; shift ;

  if [ "${L_SKIP_OPTION}" = 'y' ] ; then
    echo 'SKIPPING SED EDITS ON SRT SUBTITLE' ;
    /bin/cp "${ASS_SRC}" "${ASS_DST}" ; # Don't use '-p' to preserve this copy
    return ;
  fi

  #############################################################################
  # There might be better / more elegant ways to do this ...
  #
  # So, the editing is done using an array of PAIRs of sed regexs which are
  # fed to a sed ‘s///’ expression.  The first part of the pair is the search
  # regex, and the 2nd part is the replacement string for the match.
  #
  # The best way to see how this works is to look at the expression pairs --
  # I've included a couple of commented out more complicated examples of what
  # can be done.
  #
  # Because libass allows for and provides complex control over the subtitles, 
  # breaking up the sed command was the easiest way to manage all of the
  # complex shell escaping necessary for it to all work smoothly.  There are
  # quite a few special character collisions that have to be handled and I
  # think I've handled the ones I'm aware.  There might still be cases that
  # I missed (most probably), but I think this is pretty robust as it stands.
  #
  SED_SCRIPT_ARRAY=(
    '^Format: Name,.*'
      'Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding <==> Style: Default Italic,Open Sans Semibold,39,&H30FF00DD,&H000000FF,&H00101010,&H20A0A0A0,0,1,0,0,100,100,0,0,1,2.2,0,2,105,105,11,1'
      #########################################################################
      # Normally, these are already specified correctly for an 'ASS' subtitle.
      # These are here for the cases where ffmpeg is used to convert a 'SRT'
      # subtitle to an 'ASS' subtitle where the default value ffmpeg chooses
      # is incorrect for the video that is being encoded.
      #
    '^PlayResX:.*'
        'PlayResX: 848'   # Note - differences for PlayResX / PlayResY affect
    '^PlayResY:.*'        #        the Fontsize and Outline values, and the
        'PlayResY: 480'   #        drawing position of items in each frame.
    'Style: Default,.*'
        'Style: Default,Open Sans Semibold,39,&H6000F8FF,&H000000FF,&H00101010,&H50A0A0A0,-1,0,0,0,100,100,0,0,1,2.75,0,2,100,100,12,1'
      #########################################################################
      # - Note that the necessary _replacement_ escapes are added by the MAIN
      #   script, i.e., the '&' character does not need to be and should NOT
      #   be escaped in the replacement part.
      # - Because some libass directives are preceeded by a '\' character, e.g.
      #   '{\pos(13,80)}'.  This makes using the '\' character a little tricky
      #   Consider the following sed script pair (from a converted SRT file):
      #
    '^Dialogue: \(.*\),Default,\(.*\),{\\i1}\(.*\){\\i[0]*}$'
        'Dialogue: %%%1,Default Italic,%%%2,%%%3'
      #
      #   Its purpose is to detect a line that is all italic, and select a
      #   different style for that Dialogue line keeping everything else the
      #   same.  In this case, the \1, \2, and \3 capture buffer specifiers
      #   are represented as %%%1, %%%2, and %%%3 in the replacement string.
      #   Ugly, I know.
  ) ;

  # https://unix.stackexchange.com/questions/181937/how-create-a-temporary-file-in-shell-script
  G_TMP_FILE="$(mktemp "/tmp/${C_SCRIPT_NAME}-sed.XXXXXX")" ;
# exec 3>"${G_TMP_FILE}" ;
# exec 4<"${G_TMP_FILE}" ;
# /bin/rm -f "${G_TMP_FILE}" ;

  local ido=0 ;
  local idx=0 ;

  while [ "${SED_SCRIPT_ARRAY[$idx]}" != '' ] ; do  # {

    (( ido = (idx / 2) + 1 )) ; # A temp for displaying the regex index
    echo -n "${ATTR_BROWN_BOLD}${ido}${ATTR_OFF}." ;

    (( ido = idx + 1 )) ;

      #########################################################################
      # Insane shell escape sequences - also, the sed '-e' ordering is
      # important.  This is because some .ASS directives are preceeded by a
      # '\', e.g. {\pos(13,80)}.  Also, regex captures are complicated to code
      # -- I ended up using '%%%' to represent the '\' character.
      #
    echo -n '.' ;
    REPLACEMENT_STR=`echo "${SED_SCRIPT_ARRAY[$ido]}" \
                | ${SED} -e 's#\\\#\\\\\\\#g'   \
                       -e 's/,&H/,\\\\\&H/g'  \
                       -e 's/ <==> /\\\\n/g'  \
                       -e 's/%%%/\\\/g'
                 `;
    echo -n '. ' ;

    echo "s#${SED_SCRIPT_ARRAY[$idx]}#${REPLACEMENT_STR}#" \
      >> "${G_TMP_FILE}" ;
    (( idx += 2 )) ;
  done  # }

  #############################################################################
  # Note, if you want to use '--regexp-extended' then some of the expressions
  # need to be redone because of the syntax difference between the two modes.
  # It just might be better to include another sed script here instead.
  #
  echo -n 'DOS..'
  cat "${ASS_SRC}"  \
      | ${DOS2UNIX} \
      | ${SED} "--file=${G_TMP_FILE}" \
    > "${ASS_DST}" ;
  echo '.' ;
}


###############################################################################
###############################################################################
# SRT subtitles are always converted to ASS subtitles using ffmpeg.
# This function provides a way to edit those subtitles, if desired.
#
# This applies some simple enhancements to the ASS script built by ffmpeg.
# Usually SRT subtitles are kinda bland and this makes them a little better.
#
apply_script_to_ass_subtitles() {  # input_pathname  output_pathname

  local L_SKIP_OPTION="$1" ; shift ;
  local ASS_SRC="$1" ; shift ;
  local ASS_DST="$1" ; shift ;
  local ASS_SCRIPT="$1" ; shift ; # If = '', then we'll use a "default"

  if [ "${L_SKIP_OPTION}" = 'y' ] ; then
    echo 'SKIPPING SED EDITS ON SRT SUBTITLE' ;
    /bin/cp "${ASS_SRC}" "${ASS_DST}" ; # Don't use '-p' to preserve this copy
    return ;
  fi

  /bin/cp "${ASS_SRC}" "${ASS_DST}" ; # Don't use '-p' to preserve this copy
  return ;

  #############################################################################
  # There might be better / more elegant ways to do this ...
  #
  # So, the editing is done using an array of PAIRs of sed regexs which are
  # fed to a sed ‘s///’ expression.  The first part of the pair is the search
  # regex, and the 2nd part is the replacement string for the match.
  #
  # The best way to see how this works is to look at the expression pairs --
  # I've included a couple of commented out more complicated examples of what
  # can be done.
  #
  # Because libass allows for and provides complex control over the subtitles, 
  # breaking up the sed command was the easiest way to manage all of the
  # complex shell escaping necessary for it to all work smoothly.  There are
  # quite a few special character collisions that have to be handled and I
  # think I've handled the ones I'm aware.  There might still be cases that
  # I missed (most probably), but I think this is pretty robust as it stands.
  #
  SED_SCRIPT_ARRAY=(
    '^Format: Name,.*'
      'Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding <==> Style: Default Italic,Open Sans Semibold,39,&H30FF00DD,&H000000FF,&H00101010,&H20A0A0A0,0,1,0,0,100,100,0,0,1,2.2,0,2,105,105,11,1'
      #########################################################################
      # Normally, these are already specified correctly for an 'ASS' subtitle.
      # These are here for the cases where ffmpeg is used to convert a 'SRT'
      # subtitle to an 'ASS' subtitle where the default value ffmpeg chooses
      # is incorrect for the video that is being encoded.
      #
    '^PlayResX:.*'
        'PlayResX: 848'   # Note - differences for PlayResX / PlayResY affect
    '^PlayResY:.*'        #        the Fontsize and Outline values, and the
        'PlayResY: 480'   #        drawing position of items in each frame.
    'Style: Default,.*'
        'Style: Default,Open Sans Semibold,39,&H6000F8FF,&H000000FF,&H00101010,&H50A0A0A0,-1,0,0,0,100,100,0,0,1,2.75,0,2,100,100,12,1'
      #########################################################################
      # - Note that the necessary _replacement_ escapes are added by the MAIN
      #   script, i.e., the '&' character does not need to be and should NOT
      #   be escaped in the replacement part.
      # - Because some libass directives are preceeded by a '\' character, e.g.
      #   '{\pos(13,80)}'.  This makes using the '\' character a little tricky
      #   Consider the following sed script pair (from a converted SRT file):
      #
    '^Dialogue: \(.*\),Default,\(.*\),{\\i1}\(.*\){\\i[0]*}$'
        'Dialogue: %%%1,Default Italic,%%%2,%%%3'
      #
      #   Its purpose is to detect a line that is all italic, and select a
      #   different style for that Dialogue line keeping everything else the
      #   same.  In this case, the \1, \2, and \3 capture buffer specifiers
      #   are represented as %%%1, %%%2, and %%%3 in the replacement string.
      #   Ugly, I know.
  ) ;

  # https://unix.stackexchange.com/questions/181937/how-create-a-temporary-file-in-shell-script
  G_TMP_FILE="$(mktemp "/tmp/${C_SCRIPT_NAME}-sed.XXXXXX")" ;
# exec 3>"${G_TMP_FILE}" ;
# exec 4<"${G_TMP_FILE}" ;
# /bin/rm -f "${G_TMP_FILE}" ;

  local ido=0 ;
  local idx=0 ;

  while [ "${SED_SCRIPT_ARRAY[$idx]}" != '' ] ; do  # {

    (( ido = (idx / 2) + 1 )) ; # A temp for displaying the regex index
    echo -n "${ATTR_BROWN_BOLD}${ido}${ATTR_OFF}." ;

    (( ido = idx + 1 )) ;

      #########################################################################
      # Insane shell escape sequences - also, the sed '-e' ordering is
      # important.  This is because some .ASS directives are preceeded by a
      # '\', e.g. {\pos(13,80)}.  Also, regex captures are complicated to code
      # -- I ended up using '%%%' to represent the '\' character.
      #
    echo -n '.' ;
    REPLACEMENT_STR=`echo "${SED_SCRIPT_ARRAY[$ido]}" \
                | ${SED} -e 's#\\\#\\\\\\\#g'   \
                       -e 's/,&H/,\\\\\&H/g'  \
                       -e 's/ <==> /\\\\n/g'  \
                       -e 's/%%%/\\\/g'
                 `;
    echo -n '. ' ;

    echo "s#${SED_SCRIPT_ARRAY[$idx]}#${REPLACEMENT_STR}#" \
      >> "${G_TMP_FILE}" ;
    (( idx += 2 )) ;
  done  # }

  #############################################################################
  # Note, if you want to use '--regexp-extended' then some of the expressions
  # need to be redone because of the syntax difference between the two modes.
  # It just might be better to include another sed script here instead.
  #
  echo -n 'DOS..'
  cat "${ASS_SRC}"  \
      | ${DOS2UNIX} \
      | ${SED} "--file=${G_TMP_FILE}" \
    > "${ASS_DST}" ;
  echo '.' ;
}


###############################################################################
#        #####
#       #     #   #####    ##    #####    #####
#       #           #     #  #   #    #     #
#        #####      #    #    #  #    #     #
#             #     #    ######  #####      #
#       #     #     #    #    #  #   #      #
#        #####      #    #    #  #    #     #
###############################################################################
#
echo -n "${ATTR_BLUE_BOLD}<< " ;
echo -n "'${ATTR_GREEN_BOLD}${G_IN_FILE}${ATTR_OFF}${ATTR_BLUE_BOLD}'"
echo    " >>${ATTR_OFF} ..." ;

if [ -s "${C_VIDEO_OUT_DIR}/${G_IN_BASENAME}.${C_OUTPUT_CONTAINER}" ] ; then
  echo -n "  $(tput setaf 3 ; tput bold)COMPLETED$(tput sgr0; tput bold) "
  echo    "'${G_IN_FILE}'$(tput sgr0) ..." ;
  exit 0;
fi


###############################################################################
# See if the input video has some subtitles.
# If so, then burn them into the converted file.
#
# :: This might be the best place to see if ‘--no-subs’ was specified on the
#    command line (IOWs, this is enclosed by an IF).  Other changes have to be
#    made later in this script to ensure subtitles are NOT encoeded if that is
#    the case.
#

###############################################################################
#   #####
#  #     #  #    #  ####   #####  #  #####  #      #####   ####
#  #        #    #  #   #    #    #    #    #      #      #
#   #####   #    #  ####     #    #    #    #      ####    ####
#        #  #    #  #   #    #    #    #    #      #           #
#  #     #  #    #  #   #    #    #    #    #      #      #    #   ##  ##  ##
#   #####    ####   ####     #    #    #    #####  #####   ####    ##  ##  ##
#
# Here are the subtitle rules:
# - if '--no-subs' option is specified, any/all subtitles will be ignored and
#   will not be hard-sub'd in the video during the re-encoding;
# - next, MANUALLY added subtitles are checked (this allows for an overriding
#   of a subtitle that may be attached with the video):
#   . if there is a subtitle in 'C_SUBTITLE_IN_DIR' and it is NEWER than the
#     subtitle in 'C_SUBTITLE_OUT_DIR', then that subtitle is processed and
#     used in the re-encoding of the video.
#     > An SRT subtitle will take priority over an ASS subtitle in
#       'C_SUBTITLE_IN_DIR'.  This is because an SRT subtitle is converted to
#       an ASS subtitle in 'C_SUBTITLE_IN_DIR', then processed and saved into
#       'C_SUBTITLE_OUT_DIR' which is then used during the re-encoding process;
#   . NOTE -- for manually added subtitles it's important to ensure that all
#     of the fonts that are used in the subtitle are visible to ffmpeg.
#     This also applies to videos with embedded subtitles although it's less
#     likely that they were packaged with missing fonts.
# - and finally, if the video contains a subtitle, then that subtitle is
#   processed and used in the re-encoding of the video.
#
# None of the above will explicitely trigger a re-encoding of the video, only
# a missing or out-of-date video will trigger the re-encoding process.  These
# steps only determine if subtitles should be reprocessed for the re-encoding.
#
# ⚙️ IN OTHER WORDS, simply adding/replacing a subtitle file will not trigger
#   a re-encoding of the video by this script.
###############################################################################
#
if [ "${G_OPTION_NO_SUBS}" != 'y' ] ; then  # {

  if [ -s "${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.srt" ] ; then  # {

    ###########################################################################
    # See if this subtitle is NEWER than our previous version.  If the right
    # side of the '-nt' EXPRESSION is NOT there, then the test succeeds (i.e.,
    # the file is __newer__ than the non-existant file).  This seems logical
    # and __maybe__ implied by the man page, but there is NO explicit example
    # of this in the man page (“An omitted EXPRESSION defaults to false”).
    # Is a non-existant file considered an omitted EXPRESSION?
    #
    # Also, an expression using '-nt' is NOT reliable across different file-
    # system types because of the non-uniform increased timestamp resolution
    # between the filesystems (e.g., ext3 vs. ntfs or fat32).  This used to
    # work; dunno if it has been fixed in the interim and google returns way
    # too many results for any search terms I can think of to see if it's even
    # been identified as a bug.
    #
    if [ "${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.srt" \
     -nt "${C_SUBTITLE_OUT_DIR}/${G_IN_BASENAME}.ass" ] ; then  # { OKAY

      [ "${G_OPTION_VERBOSE}" = 'y' ] && \
        echo "${ATTR_YELLOW_BOLD}FOUND AN SRT SUBTITLE$(tput sgr0).." ;
      set -x ;
      ${FFMPEG} -i "${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.srt" \
                   "${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.ass" \
          >/dev/null 2>&1 ; RC=$? ;
      { set +x ; } >/dev/null 2>&1
      if [ ${RC} -ne 0 ] ; then
        echo -n "${ATTR_ERROR} ${FFMPEG} -i " ;
        echo -n "'${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.srt' " ;
        echo    "'${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.ass'" ;
        exit 1 ;
      fi

      G_SUBTITLE_PATHNAME="${C_SUBTITLE_OUT_DIR}/${G_IN_BASENAME}.ass"
      apply_script_to_srt_subtitles \
          "${G_OPTION_NO_MODIFY_SRT}" \
          "${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.ass" \
          "${G_SUBTITLE_PATHNAME}" ;
    else  # }{ OKAY
      vecho "${ATTR_YELLOW_BOLD}SRT SUBTITLE ALREADY PROCESSED$(tput sgr0) ..." ;

      G_SUBTITLE_PATHNAME="${C_SUBTITLE_OUT_DIR}/${G_IN_BASENAME}.ass"
    fi  # }
  elif [ -s "${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.ass" ] ; then  # }{
    # TODO FIXME  Right now, this case doesn't exist because we extract the
    #      video's subtitle directly to 'C_SUBTITLE_OUT_DIR' without any
    #      automated sed script editing.
    # TODO make an 'apply_script_to_ass_subtitles()' and a corresponding
    #      G_OPTION_NO_MODIFY_ASS switch.  It will be similar to the SRT
    #      script, except w/o the italics (maybe just 'Default' style only?).
    #
    echo "FOUND AN ASS SUBTITLE, SEE IF IT's NEWER" ; # TODO HERE HERE
      apply_script_to_ass_subtitles \
          "${G_OPTION_NO_MODIFY_ASS}" \
          "${C_SUBTITLE_IN_DIR}/${G_IN_BASENAME}.ass" \
          "${G_SUBTITLE_PATHNAME}" \
          "${G_OPTION_ASS_SCRIPT}" ;
  else  # }{
    #############################################################################
    # We have subtitles in this video, so let's see if there are any font
    # attachments in the video.  If there are, then extract those fonts to the
    # fonts' directory (in 'C_FONTS_DIR') to provide visibility to ffmpeg.
    #
    ${DBG} ${MKVMERGE} -i "${G_IN_FILE}" | grep -q 'subtitles' ; RC=$? ;
    if [ ${RC} -eq 0 ] ; then  # { OKAY

      vecho "${ATTR_YELLOW_BOLD}  SUBTITLE FOUND IN VIDEO$(tput sgr0) ..." ;

      extract_fonts_in_video "${G_IN_FILE}" "${C_FONTS_DIR}" ;
      extract_video_subtitle "${G_IN_FILE}" ;
    fi  # }
  fi  # }
else  # }{
  echo 'SUBTITLES WERE FORCED SKIPPED' ;
fi  # }


###############################################################################
# Welcome to the wonderful world of shell escape!  Pretty sure no personal
# info will be added to the video's comments, and you can easily verify that
# the comment is what you expected using VLC.
#
# There might be a cleaner/cleverer way to do this, but darn if I know how to!
#
if [ "${G_SUBTITLE_PATHNAME}" = '' ] ; then  # {
  if [ "${C_VIDEO_FILTERS}" = '' ] ; then
    eval set -- ; # Make an EMPTY eval just to keep the code simple ...
  else
    C_FFMPEG_VIDEO_FILTERS="`echo "${C_VIDEO_FILTERS}" \
                           | ${SED} -e 's#\([ :()[]\)#\\\\\\\\\\\\\1#g' \
                                  -e 's#]#\\\\\\\\\\\\\]#g'`" ;
    eval set -- "'-vf' ${C_FFMPEG_VIDEO_FILTERS}" ;
  fi
else
  G_FFMPEG_SUBTITLE_FILENAME="`echo "${G_SUBTITLE_PATHNAME}" \
                             | ${SED} -e 's#\([ :,()[]\)#\\\\\\\\\\\\\1#g' \
                                    -e 's#]#\\\\\\\\\\\\\]#g'`" ;
  G_FFMPEG_FONTS_DIR="`echo "${C_FONTS_DIR}" \
                     | ${SED} -e 's#\([ :,()[]\)#\\\\\\\\\\\\\1#g' \
                            -e 's#]#\\\\\\\\\\\\\]#g'`" ;
  if [ "${C_VIDEO_FILTERS}" = '' ] ; then
    eval set -- "'-vf' subtitles=${G_FFMPEG_SUBTITLE_FILENAME}:fontsdir=${G_FFMPEG_FONTS_DIR}" ;
  else
    C_FFMPEG_VIDEO_FILTERS="`echo "${C_VIDEO_FILTERS}" \
                           | ${SED} -e 's#\([ :()[]\)#\\\\\\\\\\\\\1#g' \
                                  -e 's#]#\\\\\\\\\\\\\]#g'`" ;
    eval set -- "'-vf' ${C_FFMPEG_VIDEO_FILTERS},subtitles=${G_FFMPEG_SUBTITLE_FILENAME}:fontsdir=${G_FFMPEG_FONTS_DIR}" ;
  fi
fi  # }


###############################################################################
#
if [ ! -s "${C_VIDEO_OUT_DIR}/${G_IN_BASENAME}.${C_OUTPUT_CONTAINER}" ] ; then

  if [ "${G_METADATA_TITLE}" = '' ] ; then
    G_METADATA_TITLE="$(echo "${G_IN_BASENAME}" | ${SED} -e 's/[_+]/ /g')" ;
  fi

  #############################################################################
  # If the comment isn't set, then build a default comment.
  #
  if [ "${C_METADATA_COMMENT}" = '' ] ; then
    C_METADATA_COMMENT="`cat <<HERE_DOC
Encoded on $(date)
$(uname -sr ; ffmpeg -version | egrep '^ffmpeg ')
${FFMPEG} -c:a libmp3lame -ab ${C_FFMPEG_MP3_BITS}K -c:v libx264 -preset ${C_FFMPEG_PRESET} -crf ${C_FFMPEG_CRF} -tune film -profile:v high -level 4.1 -pix_fmt yuv420p $(echo $@ | ${SED} -e 's/[\\]//g' -e "s#${HOME}#\\\${HOME}#g") '${G_IN_BASENAME}.${C_OUTPUT_CONTAINER}'
HERE_DOC
`" ;
  fi

  set -x ; # We want to KEEP this enabled.
  ${DBG} ${FFMPEG} -i "${G_IN_FILE}" \
                   -c:a libmp3lame -ab ${C_FFMPEG_MP3_BITS}K \
                   -c:v libx264 -preset ${C_FFMPEG_PRESET} \
                   -crf ${C_FFMPEG_CRF} \
                   -tune film -profile:v high -level 4.1 -pix_fmt yuv420p \
                   "$@" \
                   -metadata "title=${G_METADATA_TITLE}" \
                   -metadata "genre=${C_METADATA_GENRE}" \
                   -metadata "comment=${C_METADATA_COMMENT}" \
                   "file:${C_VIDEO_OUT_DIR}/${G_IN_BASENAME}.${C_OUTPUT_CONTAINER}" ;
  { set +x ; } >/dev/null 2>&1
else
  echo "$(tput setaf 3 ; tput bold)COMPLETED$(tput sgr0; tput bold) '${G_IN_FILE}'$(tput sgr0) ..." ;
fi

exit 0 ; # TODO :: should return ffmpeg's return code instead

