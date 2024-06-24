import matplotlib.pyplot as plt
import base64
from io import BytesIO

def generate_plot(x1,x2, y_r,y_l):
    """
    Generate a plot and return it as a base64-encoded string.

    Args:
        x (list): Gait cycle percentage.
        y (list): Knee angle percentage.

    Returns:
        str: The plot as a base64-encoded string.
    """
    plt.switch_backend('AGG')
    plt.figure(figsize=(15, 3))
    plt.plot(x1, y_r, color='red',label='right knee')
    plt.plot(x2, y_l, color='blue', label='left knee')
    plt.xlabel('Time (seconds)')
    plt.ylabel('Knee Angle (degrees)')
    plt.legend()
    plt.tight_layout()
    plt.style.use('ggplot')
    graph = generate_graph()

    return graph


def generate_expected_curve(x,expected_curve):
    plt.switch_backend('AGG')
    plt.figure(figsize=(15, 3))
    plt.plot(x, expected_curve, color='red',label='right knee')
    plt.xlabel('Gait Cycle (%)')
    plt.ylabel('Knee Angle (degrees)')
    plt.legend()
    plt.tight_layout()
    plt.style.use('ggplot')
    graph = generate_graph()

    return graph


def generate_graph() -> None:
    buffer = BytesIO()
    plt.savefig(buffer, format='png')
    buffer.seek(0)
    image_png = buffer.getvalue()
    graph = base64.b64encode(image_png)
    graph = graph.decode('utf-8')
    buffer.close()

    return graph