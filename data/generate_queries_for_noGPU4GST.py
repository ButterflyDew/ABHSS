from pathlib import Path
import random


ROOT = Path(__file__).resolve().parent
G_VALUES = range(10, 21)
AVERAGES = [200, 400, 800, 1600]
QUERIES_PER_AVERAGE = 10
STD_RATIO = 0.15
BASE_SEED = 20260519


def read_node_count(graph_path: Path) -> int:
    with graph_path.open("r", encoding="utf-8", errors="ignore") as graph_file:
        header = graph_file.readline().split()
    if len(header) < 2:
        raise ValueError(f"Invalid graph header: {graph_path}")
    return int(header[0])


def generate_query_file(graph_dir: Path, n: int, g: int) -> None:
    rng = random.Random(f"{BASE_SEED}:{graph_dir.name}:{g}")
    out_path = graph_dir / f"query_g{g}.txt"
    query_count = len(AVERAGES) * QUERIES_PER_AVERAGE

    with out_path.open("w", encoding="utf-8", newline="\n") as out:
        out.write(f"{query_count}\n")

        for avg in AVERAGES:
            std = max(1.0, avg * STD_RATIO)
            low = max(1, int(round(avg * 0.5)))
            high = min(n, int(round(avg * 1.5)))

            for _ in range(QUERIES_PER_AVERAGE):
                out.write(f"{g}\n")

                for _ in range(g):
                    f = int(round(rng.gauss(avg, std)))
                    f = max(low, min(high, f, n))
                    nodes = rng.sample(range(1, n + 1), f)
                    out.write(f"{f} {' '.join(map(str, nodes))}\n")


def main() -> None:
    graph_dirs = sorted(
        [path for path in ROOT.iterdir() if path.is_dir() and (path / "graph.txt").exists()],
        key=lambda path: path.name.lower(),
    )

    if not graph_dirs:
        raise FileNotFoundError("No graph.txt found in subdirectories.")

    for graph_dir in graph_dirs:
        graph_path = graph_dir / "graph.txt"
        n = read_node_count(graph_path)

        for g in G_VALUES:
            generate_query_file(graph_dir, n, g)

        print(f"Generated query_g10.txt to query_g20.txt in {graph_dir.name} (n={n})")


if __name__ == "__main__":
    main()
