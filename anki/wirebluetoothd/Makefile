.PHONY: wirebluetoothd

all: wirebluetoothd

go_deps:
	echo `${HOME}/.anki/go/dist/1.24.4/go/bin/go version` && ${HOME}/.anki/go/dist/1.24.4/go/bin/go mod download

wirebluetoothd: go_deps
	mkdir -p build
	GOARCH=arm GOARM=7 ${HOME}/.anki/go/dist/1.24.4/go/bin/go build -tags vicos -ldflags '-w -s' -o build/wirebluetoothd
	${HOME}/.anki/upx/dist/5.0.1/upx --best --lzma build/wirebluetoothd
