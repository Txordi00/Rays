import matplotlib.pyplot as plt
import numpy as np


def plot_hemisphere(normal, num_points=1000):
    """
    Generates and plots a 3D distribution of points on a sphere.

    Args:
        num_points (int): The number of points to generate.
    """
    # Change of basis to a basis where normal points upwards
    nx = normal[0]
    ny = normal[1]
    nz = normal[2]
    a = 1 / (1 + nz)
    b = -nx * ny
    S = np.array(
        [[1 - nx**2 * a, b * a, -nx], [b * a, 1 - ny**2 * a, -ny], [nx, ny, nz]]
    )

    # Generate two independent uniform random variables in [0.0, 1.0]
    u = np.random.rand(num_points)
    v = np.random.rand(num_points)

    # Calculate phi and theta
    phi = 2 * np.pi * u
    theta = np.arccos(1 - v)

    # Compute in normal local frame and move to cartesian world frame
    b1 = np.sin(theta) * np.cos(phi)
    b2 = np.sin(theta) * np.sin(phi)
    n = np.cos(theta)
    x, y, z = np.asarray(np.transpose(S) @ np.array([b1, b2, n]))

    # Plot the 3D distribution
    fig = plt.figure()
    ax = fig.add_subplot(111, projection="3d")
    ax.scatter(x, y, z, zorder=1)
    ax.quiver(0, 0, 0, normal[0], normal[1], normal[2], color="r", zorder=2)

    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.set_title("Uniform sampling of Hemisphere")
    ax.set_xlim([-1.5, 1.5])
    ax.set_ylim([-1.5, 1.5])
    ax.set_zlim([-1.5, 1.5])

    plt.show()


def concentric_sample_disk(u):
    u_offset = 2.0 * u - np.array([1.0, 1.0])
    if np.abs(u_offset[0]) < 0.001 and np.abs(u_offset[1]) < 0.001:
        return np.array([0.0, 0.0]), 0.0

    if np.abs(u_offset[0]) > np.abs(u_offset[1]):
        r = u_offset[0]
        theta = (np.pi / 4.0) * (u_offset[1] / u_offset[0])
    else:
        r = u_offset[1]
        theta = (np.pi / 2.0) - (np.pi / 4.0) * (u_offset[0] / u_offset[1])

    cos_theta = np.cos(theta)
    return r * np.array([cos_theta, np.sin(theta)]), cos_theta


def plot_hemisphere_cosweighted(normal, num_points=1000):
    # Change of basis to a basis where normal points upwards
    nx = normal[0]
    ny = normal[1]
    nz = normal[2]
    a = 1 / (1 + nz)
    b = -nx * ny
    S = np.array(
        [[1 - nx**2 * a, b * a, -nx], [b * a, 1 - ny**2 * a, -ny], [nx, ny, nz]]
    )

    # Generate two independent uniform random variables in [0.0, 1.0]
    u = np.random.rand(num_points)
    v = np.random.rand(num_points)

    print("U:")
    print(u)
    print("V:")
    print(v)

    x = np.zeros(num_points)
    y = np.zeros(num_points)
    z = np.zeros(num_points)
    for i in range(num_points):
        d, cosTheta = concentric_sample_disk(np.array([u[i], v[i]]))
        d2 = np.dot(d, d)
        zz = np.sqrt(np.max(np.array([0.0, 1.0 - d2])))
        sampleInNormalFrame = np.array([d[0], d[1], zz])
        sampleDir = np.matmul(np.transpose(S), sampleInNormalFrame)
        x[i] = sampleDir[0]
        y[i] = sampleDir[1]
        z[i] = sampleDir[2]

    # Plot the 3D distribution
    fig = plt.figure()
    ax = fig.add_subplot(111, projection="3d")
    ax.scatter(x, y, z, zorder=1)
    ax.quiver(0, 0, 0, normal[0], normal[1], normal[2], color="r", zorder=2)

    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.set_title("Cosine-weighted hemisphere sampling")
    ax.set_xlim([-1.5, 1.5])
    ax.set_ylim([-1.5, 1.5])
    ax.set_zlim([-1.5, 1.5])

    plt.show()


def plot_specular(normal, alpha, num_points=1000):
    """
    Generates and plots a 3D distribution of points on a sphere.

    Args:
        num_points (int): The number of points to generate.
    """
    # Change of basis to a basis where normal points upwards
    nx = normal[0]
    ny = normal[1]
    nz = normal[2]
    a = 1 / (1 + nz)
    b = -nx * ny
    S = np.array(
        [[1 - nx**2 * a, b * a, -nx], [b * a, 1 - ny**2 * a, -ny], [nx, ny, nz]]
    )

    # Generate two independent uniform random variables in [0.0, 1.0]
    u = np.random.rand(num_points)
    v = np.random.rand(num_points)

    # Calculate phi and theta
    phi = 2 * np.pi * u
    theta = np.arccos(np.sqrt((1 - v) / (v * (alpha**2 - 1) + 1)))

    # Compute in normal local frame and move to cartesian world frame
    b1 = np.sin(theta) * np.cos(phi)
    b2 = np.sin(theta) * np.sin(phi)
    n = np.cos(theta)
    x, y, z = np.asarray(np.transpose(S) @ np.array([b1, b2, n]))

    # Plot the 3D distribution
    fig = plt.figure()
    ax = fig.add_subplot(111, projection="3d")
    ax.scatter(x, y, z, zorder=1)
    ax.quiver(0, 0, 0, normal[0], normal[1], normal[2], color="r", zorder=2)

    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.set_title("Microfacet GGX specular r=0.25")
    ax.set_xlim([-1.5, 1.5])
    ax.set_ylim([-1.5, 1.5])
    ax.set_zlim([-1.5, 1.5])

    plt.show()


if __name__ == "__main__":
    normal = np.array([0.5, 1, 0.2])
    normal /= np.linalg.norm(normal)
    # plot_hemisphere(normal=normal)
    # plot_specular(normal=normal, alpha=0.25**2)
    # print(np.random.rand(16))
    plot_hemisphere_cosweighted(normal, num_points=32)
