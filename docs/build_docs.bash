#!/bin/bash

input_folder=""
output_folder=""

display_help() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  -i, --input INPUT_FOLDER  Specify the input folder (default: chatgpt)"
    echo "  -o, --output OUTPUT_FOLDER Specify the output folder (default: docs)"
    echo "  -h, --help                Display this help message"
    exit 0
}

if [ $# -ne 4 ]; then
    display_help
fi

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -i)
            input_folder="$2"
            shift
            shift
            ;;
        -o)
            output_folder="$2"
            shift
            shift
            ;;
        *)
            display_help
            ;;
    esac
done

if [ -z "$input_folder" ] || [ -z "$output_folder" ]; then
    display_help
fi

if [ ! -d "$input_folder" ]; then
    echo "Input folder '$input_folder' does not exist."
    exit 1
fi

mkdir -p "$output_folder"

for yaml_file in "$input_folder"/*.yaml; do
    if [[ -f "$yaml_file" ]]; then
        filename=$(basename "$yaml_file" .yaml)
        redocly build-docs "$yaml_file" -o "$output_folder/$filename.html"
    fi
done
