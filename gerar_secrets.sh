#!/usr/bin/env bash
# Le o .env e gera o secrets.h dos dois sketches (ota_boot_j1 e ota_boot_j2).
# Rodar sempre que mudar o .env, antes de compilar.
#
# Cada placa tem o seu proprio firmware (pinos e nome diferentes), entao cada uma consulta o seu
# manifesto: o MANIFEST_URL do .env termina em version.json e aqui vira version_j1.json (placa do
# jogador 1) ou version_j2.json (placa do jogador 2).
#
#   ./gerar_secrets.sh        -> para o .bin publicado: SEM Wi-Fi
#   ./gerar_secrets.sh --usb  -> para gravar pela USB: COM Wi-Fi (salva a rede na placa)
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_FILE="$DIR/.env"
CHAVES=(MANIFEST_URL)
if [[ "${1:-}" == "--usb" ]]; then
  CHAVES+=(WIFI_SSID WIFI_PASSWORD)
elif [[ -n "${1:-}" ]]; then
  echo "Uso: $0 [--usb]" >&2
  exit 1
fi
trap 'rm -f "$DIR"/ota_boot_j*/secrets.h.tmp' EXIT

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

gerar() {  # $1 = j1 ou j2
  local placa="$1" saida="$DIR/ota_boot_$1/secrets.h" valor escapado
  {
    echo "// Gerado por gerar_secrets.sh a partir do .env -- nao editar nem commitar."
    echo "#pragma once"
    for chave in "${CHAVES[@]}"; do
      if [[ -z "${valores[$chave]:-}" ]]; then
        echo "Erro: $chave nao definida no .env." >&2
        exit 1
      fi
      valor="${valores[$chave]}"
      if [[ "$chave" == MANIFEST_URL ]]; then
        if [[ "$valor" != *version.json ]]; then
          echo "Erro: MANIFEST_URL deve terminar em version.json (vira version_$placa.json)." >&2
          exit 1
        fi
        valor="${valor%version.json}version_$placa.json"
      fi
      escapado="${valor//\\/\\\\}"
      escapado="${escapado//\"/\\\"}"
      echo "#define $chave \"$escapado\""
    done
  } > "$saida.tmp"
  mv "$saida.tmp" "$saida"
}
gerar j1
gerar j2

if [[ "${1:-}" == "--usb" ]]; then
  echo "secrets.h gerado COM Wi-Fi: so para gravar pela USB, nao publique este .bin."
else
  echo "secrets.h gerado SEM Wi-Fi: pronto para o .bin publicado."
fi
