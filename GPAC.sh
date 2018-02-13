if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <circuit_name>"
	exit
fi

dir=`dirname "$1"`
file=`basename "$1" ".gpac"`

cat "$1"
echo
mkdir -p results/
build/GPACsim "${@:2}" --to-dot "results/${file}.dot" -o "results/${file}.pdf" "$1"
dot -Tsvg "results/${file}.dot" -o "results/${file}.svg"
#evince "results/${file}.pdf"&
#inkscape "results/${file}.svg"
