SRS_UTEST=NO
SRS_VALGRIND=NO

function show_help() {
    cat << END

Features:
  -h, --help                Print this message and exit 0.

  --utest=on|off            Whether build the utest module.

Performance:
  --valgrind=on|off         Whether build with valgrind support.
  
END
}

function parse_user_option() {
    # Parse options to variables.
    case "$option" in
        --with-utest)                   SRS_UTEST=YES               ;;
        --without-utest)                SRS_UTEST=NO                ;;
        --utest)                        SRS_UTEST=$(switch2value $value) ;;

        --with-valgrind)                SRS_VALGRIND=YES            ;;
        --without-valgrind)             SRS_VALGRIND=NO             ;;
        --valgrind)                     SRS_VALGRIND=$(switch2value $value) ;;
    *)
        echo "$0: error: invalid option \"$option\""
        exit 1
    ;;
    esac
}

function parse_user_option_to_value_and_option() {
    case "$option" in
        -*=*) 
            value=`echo "$option" | sed -e 's|[-_a-zA-Z0-9/]*=||'`
            option=`echo "$option" | sed -e 's|=[-_a-zA-Z0-9/. +]*||'`
        ;;
           *) value="" ;;
    esac
}

# For user options, only off or on(by default).
function switch2value() {
    if [[ $1 == off ]]; then
      echo NO;
    else
      echo YES;
    fi
}

#####################################################################################
# parse preset options
#####################################################################################
opt=

for option
do
    opt="$opt `echo $option | sed -e \"s/\(--[^=]*=\)\(.* .*\)/\1'\2'/\"`"
    parse_user_option_to_value_and_option
    parse_user_option
done