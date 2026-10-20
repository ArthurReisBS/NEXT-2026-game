#!/usr/bin/env bash
# Le o .env e gera ota_boot/secrets.h, que o sketch inclui.
# Rodar sempre que mudar o .env, antes de compilar.
#
#   ./gerar_secrets.sh        -> para o .bin publicado: SEM Wi-Fi
#   ./gerar_secrets.sh --usb  -> para gravar pela USB: COM Wi-Fi (salva a rede na placa)
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_FILE="$DIR/.env"
SAIDA="$DIR/ota_boot/secrets.h"
CHAVES=(MANIFEST_URL JOGADOR)
if [[ "${1:-}" == "--usb" ]]; then
  CHAVES+=(WIFI_SSID WIFI_PASSWORD)
elif [[ -n "${1:-}" ]]; then
  echo "Uso: $0 [--usb]" >&2
  exit 1
fi
trap 'rm -f "$SAIDA.tmp"' EXIT

if [[ ! -f "$ENV_FILE" ]]; then
  echo "Erro: $ENV_FILE nao existe. Copie o .env.example para .env e preencha." >&2
  exit 1
fi

declare -A valores
while IFS= read -r linha || [[ -n "$linha" ]]; do
  linha="${linha%$'\r'}"
  [[ -z "$linha" || "$linha" == \#* ]] && continue
  chave="${linha%%=*}"
  valor="${linha#*=}"
  # tira aspas em volta, se houver
  if [[ "$valor" =~ ^\"(.*)\"$ || "$valor" =~ ^\'(.*)\'$ ]]; then
    valor="${BASH_REMATCH[1]}"
  fi
  valores["$chave"]="$valor"
done < "$ENV_FILE"

{
  echo "// Gerado por gerar_secrets.sh a partir do .env -- nao editar nem commitar."
  echo "#pragma once"
  for chave in "${CHAVES[@]}"; do
    if [[ -z "${valores[$chave]:-}" ]]; then
      echo "Erro: $chave nao definida no .env." >&2
      exit 1
    fi
    escapado="${valores[$chave]//\\/\\\\}"
    escapado="${escapado//\"/\\\"}"
    echo "#define $chave \"$escapado\""
  done
} > "$SAIDA.tmp"
mv "$SAIDA.tmp" "$SAIDA"

if [[ "${1:-}" == "--usb" ]]; then
  echo "secrets.h gerado COM Wi-Fi: so para gravar pela USB, nao publique este .bin."
else
  echo "secrets.h gerado SEM Wi-Fi: pronto para o .bin publicado."
fi
