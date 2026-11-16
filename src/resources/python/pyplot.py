"""QSNote pyplot worker: source on stdin, structured result in argv[1]."""
import base64
import builtins
import io
import json
import sys
import traceback
import weakref


def render(source):
    import matplotlib
    matplotlib.use("Agg", force=True)
    import matplotlib.pyplot as plt
    from matplotlib.figure import Figure
    from matplotlib._pylab_helpers import Gcf

    matplotlib.rcParams["figure.dpi"] = 150
    created = []
    original_init = Figure.__init__

    def remember(figure, *args, **kwargs):
        original_init(figure, *args, **kwargs)
        created.append(weakref.ref(figure))

    Figure.__init__ = remember
    plt.show = lambda *args, **kwargs: None
    # Restrict direct imports in the code block; library imports use normal builtins.
    # This is an API restriction, not a Python sandbox.
    def import_module(name, globals=None, locals=None, fromlist=(), level=0):
        if name.split(".", 1)[0] in {"os", "sys", "posix", "nt", "subprocess", "ctypes", "multiprocessing"}:
            raise ImportError("System interface imports are disabled in pyplot blocks: " + name)
        return builtins.__import__(name, globals, locals, fromlist, level)

    block_builtins = dict(vars(builtins), __import__=import_module)
    exec(compile(source, "<pyplot>", "exec"), {
        "__name__": "__main__", "__file__": "<pyplot>", "__builtins__": block_builtins,
    })
    opened = {manager.canvas.figure for manager in Gcf.get_all_fig_managers()}
    figure = next((ref() for ref in reversed(created) if ref() in opened), None)
    if figure is None:
        raise RuntimeError("No open figure. Create a figure with matplotlib.pyplot before the block ends.")
    output = io.BytesIO()
    figure.savefig(output, format="png")
    return {"png": base64.b64encode(output.getvalue()).decode("ascii")}


def main():
    destination = sys.argv[1]
    source = sys.stdin.buffer.read().decode("utf-8")
    try:
        result = render(source)
    except BaseException:
        result = {"error": traceback.format_exc()}
    with open(destination, "w", encoding="utf-8") as output:
        json.dump(result, output)


if __name__ == "__main__":
    main()
