$$
\begin{align}
   [\text{Prog}] &\to [\text{Statement}]^*\\
   [\text{Statement}] &\to 
   \begin{cases}
      \text{exit([\text{Expr}])}; \\
      \text{return([\text{Expr}])}; \\
      \text{print([\text{Str lit}])}; \\
      \text{have\space\text{Identifier} = [\text{Expr}]};\\
      function\space \text{Identifier([params])} & \text{[return]}\\
      \{[\text{Statement}]^*\}
   \end{cases} \\
   [\text{Function}] &\to
   \begin{cases}
      \text{kw:[fn, func]}\\
      \text{params:[int, char]}\\
      \text{return:[:, ->][int, char]} & optional
   \end{cases}\\
   [\text{Expr}] &\to
   \begin{cases}
      [\text{Term}] \\
      [\text{BinExpr}] \\
      [\text{LogicExpr}] \\
      [\text{CmpExpr}] \\
   \end{cases}\\
   [\text{BinExpr}] &\to
   \begin{cases}
      [\text{Expr}] ^ [\text{Expr}] & \text{prec}  = 3\\
      [\text{Expr}] * [\text{Expr}] & \text{prec}  = 2\\
      [\text{Expr}] / [\text{Expr}] & \text{prec}  = 2\\
      [\text{Expr}] \% [\text{Expr}] & \text{prec} = 2\\
      [\text{Expr}] + [\text{Expr}] & \text{prec}  = 1\\
      [\text{Expr}] - [\text{Expr}] & \text{prec}  = 1\\
   \end{cases}\\
  [\text{LogicExpr}] &\to
  \begin{cases}
      [\text{Expr}] \&\& [\text{Expr}] & \text{prec} = 2\\
      [\text{Expr}] || [\text{Expr}] & \text{prec} = 1\\
  \end{cases}\\
  [\text{CmpExpr}] &\to
   \begin{cases}
      [\text{Expr}] > [\text{Expr}] \\
      [\text{Expr}] < [\text{Expr}] \\
      [\text{Expr}] >= [\text{Expr}] \\
      [\text{Expr}] <= [\text{Expr}] \\
      [\text{!Statement}] \\
      [\text{Expr}] != [\text{Expr}] \\
      [\text{Expr}] == [\text{Expr}] \\

   \end{cases} \\
  [\text{Term}] &\to
   \begin{cases}
      \text{int\_lit} \\
      \text{char\_lit} \\
      \text{str\_lit} \\
      \text{Identifier}\\
      ([\text{Expr}])\\
   \end{cases}
\end{align}
$$