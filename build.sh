#!/bin/bash

launch() {
	gnome-terminal -- bash -ic "\
		start=\$(date +%s%3N);
		$1;
		ret=\$?;

		end=\$(date +%s%3N);
		duration_ms=\$((end - start));
		minutes=\$((duration_ms / 60000));
		seconds=\$(( (duration_ms % 60000) / 1000 ));
		millis=\$((duration_ms % 1000));
		formatted_time=\$(printf \"%02d:%02d.%03d\" \$minutes \$seconds \$millis);

		echo -ne \"\n\n\n\";
		echo -ne \"Process returned code \$ret (0x\$(printf \"%08X\" \$ret))\";
		echo \" in \$formatted_time seconds.\";
		echo -ne \"Press any key to continue...\";

		if [ $2 -eq 1 ]; then
			echo \$ret > \"temp\"
		fi

		if [ \$ret -ne 0 ] || [ $2 -eq 0 ]; then
			read -n 1;
		fi
	"

	if [ $2 -eq 1 ]; then
		while [ ! -f "temp" ]; do
			sleep 0.1
		done

		ret=$(cat "temp")
		rm "temp"

		return $ret
	fi

	return 0
}

launch "ninja -C build" "1"
ret=$?

serverArgs="0.0.0.0   4023"
clientArgs="127.0.0.1 4023 johnsmith 1234"

if [ $ret -eq 0 ]; then

	ps aux | grep -P " ./bin/server " | grep -v "grep"
	if [ $? -ne 0 ]; then
		launch "./bin/server $serverArgs" "0"
	fi

	sleep 0.100
	launch "./bin/client $clientArgs" "0"
fi
