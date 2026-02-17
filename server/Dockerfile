FROM golang:1.22-bookworm AS builder

WORKDIR /src
COPY go.mod go.sum* ./
RUN go mod download || true
COPY . .
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o /out/stv-server ./cmd/stv-server

FROM gcr.io/distroless/base-debian12
COPY --from=builder /out/stv-server /usr/local/bin/stv-server
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/stv-server"]
