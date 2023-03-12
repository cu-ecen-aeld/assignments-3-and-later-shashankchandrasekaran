

case "$1" in
    start)
        echo "Starting dameon process - aesdsocket"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    
    stop)
         echo "Stoping daemon process - aesdsocket"
         start-stop-daemon -K -n aesdsocket
         ;;
    *)

    echo "Usage: $0{start|stop}"

    exit 1
esac

exit 0
