if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <circuit_name>"
	exit
fi

cat "circuits/$1"
build/GPACsim "${@:2}" --to-dot "$1.dot" -o "$1.pdf" "circuits/$1"
dot -Tsvg "$1.dot" -o "$1.svg"
evince "$1.pdf"&
inkscape "$1.svg"
