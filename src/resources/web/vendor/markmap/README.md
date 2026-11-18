Markmap 1.1.1 from https://github.com/markmap/markmap.
The bundled runtime and its dependencies are covered by LICENSES.txt.

To rebuild, copy entry.js into an empty directory and run:

    npm install @markmap/parse@1.1.1 @markmap/render@1.1.1 @markmap/view@1.1.1 esbuild@0.28.2
    npx esbuild entry.js --bundle --minify --target=chrome100 --global-name=qsnoteMarkmap --outfile=markmap.min.js

Copy markmap.min.js back here and refresh the dependency notices if versions change.
