mkdir -p build
cd build
cmake ..
make -j
cd ..

if [[ $# -lt 1 ]]; then
	echo "Usage: $0 <circuit_name>"
	exit
fi

dir=`dirname "$1"`
file=`basename "$1" ".gpac"`

cat "$1"
echo
mkdir -p results/
build/GPACsim "${@:2}" --to-dot "results/${file}.dot" --to-latex "results/${file}_equation.tex" -o "results/${file}.pdf" "$1"
dot -Tsvg "results/${file}.dot" -o "results/${file}.svg"&
pdflatex "results/${file}_equation.tex" > /dev/null
mv "${file}_equation.pdf" results/
rm -f ${file}_equation.*
#evince "results/${file}.pdf"&
#inkscape "results/${file}.svg"
