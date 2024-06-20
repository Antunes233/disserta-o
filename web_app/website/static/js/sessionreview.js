const mynew = document.getElementById("container");
const myimg = document.createElement("main_plot");

mynew.addEventListener("mousemove", onZoom);
mynew.addEventListener("mouseleave", offZoom);
mynew.addEventListener("mouseover", onZoom);

function onZoom(e) {
    const x = e.clientX - e.target.offsetLeft;
    const y = e.clientY - e.target.offsetTop;

    myimg.style.transformOrigin= `${x}px, ${y}px`;
    myimg.style.transform = `scale(${2})`;
}

function offZoom(e) {
    myimg.style.transformOrigin= `center center`;
    myimg.style.transform = `scale(${1})`;
}