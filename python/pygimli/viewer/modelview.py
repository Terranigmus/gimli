# -*- coding: utf-8 -*-
"""
    pygimli model viewer functions.
"""

import matplotlib.pyplot as plt
import numpy as np

import matplotlib as mpl
from matplotlib.patches import Rectangle
from matplotlib.collections import PatchCollection
from matplotlib.colors import LogNorm


def drawModel1D(ax, thickness, values, plotfunction='plot',
                xlabel='Resistivity $[\Omega$ m$]$', *args, **kwargs):
    """Draw 1d block model into axis ax defined by values and thickness vectors
    using plotfunction."""

    nLayers = len(thickness) + 1
    px = np.zeros(nLayers * 2)
    pz = np.zeros(nLayers * 2)
    z1 = np.cumsum(thickness)

    for i in range(nLayers):
        px[2 * i] = values[i]
        px[2 * i + 1] = values[i]

        if i == nLayers - 1:
            pz[2 * i + 1] = z1[i - 1] * 1.2
        else:
            pz[2 * i + 1] = z1[i]
            pz[2 * i + 2] = z1[i]

    if plotfunction == 'loglog' or plotfunction == 'semilogy':
        pz[0] = thickness[0] * 0.8

    try:
        plot = getattr(ax, plotfunction)
        plot(px, pz, *args, **kwargs)
    except Exception as e:
        print(e)

    ax.set_ylabel('Depth [m]')
    ax.set_xlabel(xlabel)
    ax.set_ylim(pz[-1], pz[0])
    ax.grid()
    return ax

# def draw1dmodel(... )


def draw1dmodel(x, thk=None, xlab=None, zlab="z in m", islog=True, z0=0):
    """
        DEPRECATED
    """
    print("STYLE_WARNING!!!!!!! don't use this call. Use show1dmodel instead.")
    show1dmodel(x, thk, xlab, zlab, islog, z0)


def show1dmodel(x, thk=None, xlab=None, zlab="z in m", islog=True, z0=0):
    """draw 1d block model defined by value and thickness vectors."""
    if xlab is None:
        xlab = "$\\rho$ in $\\Omega$m"

    if thk is None:  # gimli blockmodel (thk+x together) given
        nl = int(np.floor((len(x) - 1) / 2.)) + 1
        thk = np.asarray(x)[:nl - 1]
        x = np.asarray(x)[nl - 1:nl * 2 - 1]

    z1 = np.concatenate(([0], np.cumsum(thk))) + z0
    z = np.concatenate((z1, [z1[-1] * 1.2]))
    nl = len(x)  # x.size()
    px = np.zeros((nl * 2, 1))
    pz = np.zeros((nl * 2, 1))
    for i in range(nl):
        px[2 * i] = x[i]
        px[2 * i + 1] = x[i]
        pz[2 * i + 1] = z[i + 1]
        if i < nl - 1:
            pz[2 * i + 2] = z[i + 1]

#    plt.cla()
    if islog:
        plt.semilogx(px, pz)
    else:
        plt.plot(px, pz)

    plt.ion()
    plt.grid(which='both')
    plt.xlim((np.min(x) * 0.9, np.max(x) * 1.1))
    plt.ylim((max(z1) * 1.15, 0.))
    plt.xlabel(xlab)
    plt.ylabel(zlab)
    plt.show()
    return


def showStitchedModels(models, x=None, cmin=None, cmax=None,
                       islog=True, cmap=None, title=None):
    """
        Show several 1d block models as (stitched) section.
    """
    if x is None:
        x = np.arange(len(models))

    nlay = int(np.floor((len(models[0]) - 1) / 2.)) + 1
    if cmin is None or cmax is None:
        cmin = 1e9
        cmax = 1e-9
        for model in models:
            res = np.asarray(model)[nlay - 1:nlay * 2 - 1]
            cmin = min(cmin, min(res))
            cmax = max(cmax, max(res))

    dx = np.diff(x)
    dx = np.hstack((dx, dx[-1]))
    x1 = x - dx / 2
    fig, ax = plt.subplots()

    ax.plot(x, x * 0., 'k.')
    maxz = 0.
    recs = []
    RES = []
    for i, mod in enumerate(models):
        mod1 = np.asarray(mod)
        res = mod1[nlay - 1:]
        RES.extend(res)

        thk = mod1[:nlay-1]
        thk = np.hstack((thk, thk[-1]))
        z = np.hstack((0., np.cumsum(thk)))
        maxz = max(maxz, z[-1])
        for j in range(len(thk)):
            recs.append(Rectangle((x1[i], z[j]), dx[i], thk[j]))

    pp = PatchCollection(recs)
    col = ax.add_collection(pp)
    pp.set_edgecolor(None)
    pp.set_linewidths(0.0)
    if cmap is not None:
        pp.set_cmap(cmap)

    pp.set_norm(LogNorm(cmin, cmax))
    pp.set_array(np.array(RES))
    pp.set_clim(cmin, cmax)
    ax.set_ylim((maxz, 0.))
    ax.set_xlim((x1[0], x1[-1] + dx[-1]))

    plt.colorbar(pp, ax=ax, orientation='horizontal')

    return fig, ax


def showfdemsounding(freq, inphase, quadrat, response=None, npl=2):
    """
        Show FDEM sounding as real(inphase) and imaginary (quadrature) fields
        normalized by the (purely real) free air solution.
    """
    nf = len(freq)
    fig = plt.figure(1)
    fig.clf()
    ax1 = fig.add_subplot(1, npl, npl - 1)
    plt.semilogy(inphase, freq, 'x-')
    if response is not None:
        plt.semilogy(np.asarray(response)[:nf], freq, 'x-')

    plt.grid(which='both')
    ax2 = fig.add_subplot(1, npl, npl)
    plt.semilogy(quadrat, freq, 'x-')
    if response is not None:
        plt.semilogy(np.asarray(response)[nf:], freq, 'x-')
    plt.grid(which='both')
    fig.show()
    ax = [ax1, ax2]
    if npl > 2:
        ax3 = fig.add_subplot(1, npl, 1)
        ax.append(ax3)

    return ax
